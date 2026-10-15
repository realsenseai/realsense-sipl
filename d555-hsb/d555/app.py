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

"""Application scaffolding shared by the D555 examples."""

import logging

import hololink as hololink_module
import holoscan

from .profiles import MAX_PAYLOAD_MBPS, STREAMS, total_payload_mbps
from .receivers import add_receiver_arguments, release_cuda


class D555Application(holoscan.core.Application):
    """A Holoscan application whose graph is built by compose_fn(app)."""

    def __init__(self, cuda_context, compose_fn):
        super().__init__()
        self.cuda_context = cuda_context
        self._compose_fn = compose_fn
        # (FrameStatsOp, D555Stream, frame_limit) per stream, filled in by compose_fn.
        self.streams = []

    def compose(self):
        self._compose_fn(self)


def add_common_arguments(parser):
    add_receiver_arguments(parser)
    parser.add_argument("--headless", action="store_true", help="run without a display")
    parser.add_argument("--fullscreen", action="store_true", help="run in fullscreen mode")
    parser.add_argument(
        "--frame-limit",
        type=int,
        default=None,
        help="stop after this many receiver ticks per stream; a tick also passes once a "
        "second while no frame arrives",
    )
    parser.add_argument(
        "--allow-overload",
        action="store_true",
        help=f"run stream sets above {MAX_PAYLOAD_MBPS} Mbps of payload anyway",
    )
    parser.add_argument(
        "--stats-every", type=int, default=150, help="log the frame rate every N frames (0: off)"
    )
    parser.add_argument("--log-level", type=int, default=logging.INFO, help="Python log level")


def setup_logging(args):
    logging.basicConfig(level=args.log_level, format="%(asctime)s %(levelname)s %(message)s")
    hololink_module.logging_level(args.log_level)


def validate_profiles(parser, profiles, allow_overload):
    """Exit with a usage error on an unknown profile or a stream set the camera can't send."""
    for name, index in profiles.items():
        if index not in STREAMS[name].profiles:
            parser.error(f"{name} profile must be one of {sorted(STREAMS[name].profiles)}")
    total = total_payload_mbps(profiles)
    if total > MAX_PAYLOAD_MBPS:
        text = (
            f"{total:.0f} Mbps of payload is over the camera's {MAX_PAYLOAD_MBPS} Mbps limit; "
            "expect dropped frames"
        )
        if not allow_overload:
            parser.error(text + " (--allow-overload to run anyway)")
        logging.warning(text)


def run(app, hololink, device):
    """Run the application, log each stream's RESULT line, and return the exit code."""
    app.enable_metadata(True)
    hololink.start()
    try:
        app.run()
    finally:
        hololink.stop()
        release_cuda(device)
    passed = True
    for stats, camera, frame_limit in app.streams:
        line, ok = stats.result(frame_limit, camera.stop_failed)
        logging.info(line)
        passed &= ok
    return 0 if passed else 1
