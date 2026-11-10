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

"""Show one RealSense D555 stream (depth or RGB) received through Holoscan Sensor Bridge.

python3 d555_player.py --stream depth
python3 d555_player.py --stream rgb --profile 6 --receiver roce
python3 d555_player.py --stream depth --headless --frame-limit 300
"""

import argparse
import sys

import holoscan
from d555.app import D555Application, add_common_arguments, run, setup_logging, validate_profiles
from d555.camera import find_camera, open_stream
from d555.operators import D555DecoderOp, FrameStatsOp
from d555.profiles import DEFAULT_PROFILE, STREAMS, describe
from d555.receivers import frame_condition, init_cuda, make_receiver


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--stream", choices=sorted(STREAMS), default="depth")
    parser.add_argument(
        "--profile", type=int, default=None, help="profile index (default: 640x360 at 30 fps)"
    )
    add_common_arguments(parser)
    args = parser.parse_args()
    setup_logging(args)

    stream = args.stream
    profile = DEFAULT_PROFILE[stream] if args.profile is None else args.profile
    validate_profiles(parser, {stream: profile}, args.allow_overload)
    width, height, _ = STREAMS[stream].profiles[profile]

    channel, camera = open_stream(find_camera(args.hololink), stream, profile)
    device, cuda_context = init_cuda(args.gpu)

    def compose(app):
        condition = frame_condition(app, stream, args.frame_limit)
        receiver = make_receiver(app, args, stream, profile, channel, camera, condition)
        decoder = D555DecoderOp(
            app,
            name=f"decoder_{stream}",
            stream=stream,
            width=width,
            height=height,
            out_tensor_name=stream,
        )
        stats = FrameStatsOp(
            app,
            name=f"stats_{stream}",
            stream=stream,
            width=width,
            height=height,
            stats_every=args.stats_every,
        )
        visualizer = holoscan.operators.HolovizOp(
            app,
            name="holoviz",
            headless=args.headless,
            fullscreen=args.fullscreen,
            framebuffer_srgb=False,
            width=width,
            height=height,
            window_title=f"D555 {describe(stream, profile)}",
            tensors=[
                holoscan.operators.HolovizOp.InputSpec(
                    stream, holoscan.operators.HolovizOp.InputType.COLOR
                )
            ],
        )
        app.add_flow(receiver, decoder, {("output", "input")})
        app.add_flow(receiver, stats, {("output", "input")})
        app.add_flow(decoder, visualizer, {("output", "receivers")})
        app.streams.append((stats, camera, args.frame_limit))

    return run(D555Application(cuda_context, compose), channel.hololink(), device)


if __name__ == "__main__":
    sys.exit(main())
