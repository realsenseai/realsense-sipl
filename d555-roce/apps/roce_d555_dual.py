# SPDX-License-Identifier: Apache-2.0
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
"""D555e depth AND RGB at the same time over RoCEv2.

Two data channels from one enumeration (depth: sensor 0 / VP 0x1000, RGB: sensor 1 / VP 0x1080),
each with its own RoceReceiverOp and frame checker. Runs inside the HSB 2.7 demo container.

    python3 roce_d555_dual.py --ibv-name roceP4p3s0f1 --frame-limit 900
    python3 roce_d555_dual.py --ibv-name roceP4p3s0f1 --gpu GPU-<dGPU uuid> --stats-every 0
"""

import argparse
import logging
import sys

import holoscan

import hololink as hololink_module
from d555_profiles import DEFAULT_PROFILE, STREAMS
from roce_d555_single import (RoceD555App, add_common_args, add_stream, check_bandwidth,
                              find_d555, init_cuda, open_stream, run)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--depth-profile", type=int, default=DEFAULT_PROFILE["depth"])
    parser.add_argument("--rgb-profile", type=int, default=DEFAULT_PROFILE["rgb"])
    add_common_args(parser)
    args = parser.parse_args()
    logging.basicConfig(level=args.log_level, format="%(asctime)s %(levelname)s %(message)s")
    hololink_module.logging_level(args.log_level)

    profiles = {"depth": args.depth_profile, "rgb": args.rgb_profile}
    for name, profile in profiles.items():
        if profile not in STREAMS[name].profiles:
            parser.error(f"--{name}-profile: one of {sorted(STREAMS[name].profiles)}")
    check_bandwidth(parser, profiles, args.allow_overload)
    enumeration = find_d555(args.hololink)
    streams = {name: open_stream(enumeration, name, profile) for name, profile in profiles.items()}
    ibv_name = args.ibv_name or hololink_module.infiniband_devices()[0]
    device, context = init_cuda(args.gpu)

    def compose(app):
        for name, (channel, camera) in streams.items():
            add_stream(app, name, channel, camera, profiles[name], ibv_name, args.ibv_port,
                       args.frame_limit, stats_every=args.stats_every)

    app = RoceD555App(context, compose)
    # each RoceReceiverOp blocks waiting for its next frame, so give every operator a thread
    app.scheduler(holoscan.schedulers.EventBasedScheduler(
        app, name="scheduler", worker_thread_number=4))
    return run(app, streams["depth"][0].hololink(), device)


if __name__ == "__main__":
    sys.exit(main())
