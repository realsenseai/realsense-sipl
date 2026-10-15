# SPDX-FileCopyrightText: Copyright (c) 2026, RealSense AI. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""CUDA setup and the two stock Holoscan Sensor Bridge receivers the D555 works with.

The D555 sends each frame as RoCEv2 RDMA writes, so either receiver takes it unchanged:

- ``roce``: RoceReceiverOp, which needs an RDMA-capable NIC such as a ConnectX-7 (IGX).
- ``linux``: LinuxReceiverOperator, which parses the same packets in software on any NIC
  (for example the Jetson AGX Thor's MGBE ports).
"""

import logging
import os

import cuda.bindings.driver as cuda
import hololink as hololink_module
import holoscan

from .profiles import BYTES_PER_PIXEL, STREAMS

RECEIVERS = ("linux", "roce")


def init_cuda(gpu=None):
    """Retain the primary CUDA context; returns (device, context).

    gpu is a CUDA_VISIBLE_DEVICES value. On IGX, passing a discrete GPU's UUID (nvidia-smi -L)
    makes RoceReceiverOp write frames straight into that GPU's memory.
    """
    if gpu is not None:
        # Must be set before anything in this process initializes CUDA.
        os.environ["CUDA_VISIBLE_DEVICES"] = gpu

    def check(result, *values):
        if result != cuda.CUresult.CUDA_SUCCESS:
            raise RuntimeError(f"CUDA call failed: {result}")
        return values[0] if len(values) == 1 else values

    check(*cuda.cuInit(0))
    device = check(*cuda.cuDeviceGet(0))
    name = check(*cuda.cuDeviceGetName(64, device)).rstrip(b"\0").decode()
    logging.info(f"CUDA device: {name}")
    return device, check(*cuda.cuDevicePrimaryCtxRetain(device))


def release_cuda(device):
    cuda.cuDevicePrimaryCtxRelease(device)


def add_receiver_arguments(parser):
    parser.add_argument(
        "--receiver",
        choices=RECEIVERS,
        default="linux",
        help="linux: software receiver on any NIC; roce: RoceReceiverOp on an RDMA NIC",
    )
    parser.add_argument("--hololink", default="192.168.0.2", help="camera IP address")
    parser.add_argument(
        "--ibv-name", default=None, help="RDMA device for --receiver roce (default: the first)"
    )
    parser.add_argument("--ibv-port", type=int, default=1, help="RDMA port for --receiver roce")
    parser.add_argument(
        "--gpu",
        default=None,
        help="CUDA_VISIBLE_DEVICES value for the frame buffers, e.g. a discrete GPU's UUID",
    )


def linux_receiver_affinity(index):
    """CPU set for the index-th LinuxReceiverOperator in the application.

    HSB pins every LinuxReceiverOperator thread to core $HOLOLINK_AFFINITY (default 2), so two
    receivers would share one core. Give each its own: core HOLOLINK_AFFINITY + index. An empty
    HOLOLINK_AFFINITY keeps HSB's meaning, no pinning at all.
    """
    affinity = os.getenv("HOLOLINK_AFFINITY", "2")
    if not affinity:
        return None
    return {(int(affinity) + index) % os.cpu_count()}


def make_receiver(app, args, name, profile, channel, camera, condition, index=0):
    """Create the receiver operator for one stream, as selected by --receiver.

    index numbers the receivers in the application, from 0, and spreads LinuxReceiverOperator
    threads over separate cores.
    """
    width, height, _ = STREAMS[name].profiles[profile]
    common = dict(
        name=f"receiver_{name}",
        frame_size=width * height * BYTES_PER_PIXEL,
        frame_context=app.cuda_context,
        hololink_channel=channel,
        device=camera,
    )
    if args.receiver == "roce":
        ibv_name = args.ibv_name
        if not ibv_name:
            devices = hololink_module.infiniband_devices()
            if not devices:
                raise RuntimeError(
                    "--receiver roce needs an RDMA device and none was found; "
                    "use --receiver linux"
                )
            ibv_name = devices[0]
        return hololink_module.operators.RoceReceiverOp(
            app, condition, ibv_name=ibv_name, ibv_port=args.ibv_port, **common
        )
    return hololink_module.operators.LinuxReceiverOperator(
        app, condition, receiver_affinity=linux_receiver_affinity(index), **common
    )


def frame_condition(app, name, frame_limit):
    """CountCondition for --frame-limit, otherwise run until the window is closed."""
    if frame_limit:
        return holoscan.conditions.CountCondition(app, name=f"count_{name}", count=frame_limit)
    return holoscan.conditions.BooleanCondition(app, name=f"ok_{name}", enable_tick=True)
