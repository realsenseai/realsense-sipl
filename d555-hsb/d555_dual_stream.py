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

"""Show RealSense D555 depth and RGB side by side, received through Holoscan Sensor Bridge.

Each stream gets its own data channel and receiver from one camera enumeration.

    python3 d555_dual_stream.py
    python3 d555_dual_stream.py --receiver roce --depth-profile 0 --rgb-profile 0
"""

import argparse
import sys

import holoscan
from d555.app import D555Application, add_common_arguments, run, setup_logging, validate_profiles
from d555.camera import find_camera, open_stream
from d555.operators import D555DecoderOp, FrameStatsOp
from d555.profiles import DEFAULT_PROFILE, STREAMS
from d555.receivers import frame_condition, init_cuda, make_receiver

# Left to right in the window.
LAYOUT = ("rgb", "depth")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--depth-profile", type=int, default=DEFAULT_PROFILE["depth"])
    parser.add_argument("--rgb-profile", type=int, default=DEFAULT_PROFILE["rgb"])
    add_common_arguments(parser)
    args = parser.parse_args()
    setup_logging(args)

    profiles = {"depth": args.depth_profile, "rgb": args.rgb_profile}
    validate_profiles(parser, profiles, args.allow_overload)
    # Holoviz draws when it has a frame from both decoders, so both streams run at its pace; a
    # faster stream would lose frames and fail its gap check.
    if len({STREAMS[name].profiles[index][2] for name, index in profiles.items()}) > 1:
        parser.error("--depth-profile and --rgb-profile must have the same frame rate")

    enumeration = find_camera(args.hololink)
    streams = {name: open_stream(enumeration, name, profiles[name]) for name in LAYOUT}
    device, cuda_context = init_cuda(args.gpu)

    def compose(app):
        specs = []
        for column, name in enumerate(LAYOUT):
            view = holoscan.operators.HolovizOp.InputSpec.View()
            view.offset_x = column / len(LAYOUT)
            view.offset_y = 0
            view.width = 1 / len(LAYOUT)
            view.height = 1
            spec = holoscan.operators.HolovizOp.InputSpec(
                name, holoscan.operators.HolovizOp.InputType.COLOR
            )
            spec.views = [view]
            specs.append(spec)
        width = max(STREAMS[n].profiles[profiles[n]][0] for n in LAYOUT)
        height = max(STREAMS[n].profiles[profiles[n]][1] for n in LAYOUT)
        visualizer = holoscan.operators.HolovizOp(
            app,
            name="holoviz",
            headless=args.headless,
            fullscreen=args.fullscreen,
            framebuffer_srgb=False,
            width=width * len(LAYOUT),
            height=height,
            window_title="D555 RGB and depth",
            tensors=specs,
        )
        # Both decoders feed the one visualizer; their metadata is merged rather than rejected.
        visualizer.metadata_policy = holoscan.core.MetadataPolicy.UPDATE

        for index, name in enumerate(LAYOUT):
            channel, camera = streams[name]
            width, height, _ = STREAMS[name].profiles[profiles[name]]
            condition = frame_condition(app, name, args.frame_limit)
            receiver = make_receiver(
                app, args, name, profiles[name], channel, camera, condition, index=index
            )
            decoder = D555DecoderOp(
                app,
                name=f"decoder_{name}",
                stream=name,
                width=width,
                height=height,
                out_tensor_name=name,
            )
            stats = FrameStatsOp(
                app,
                name=f"stats_{name}",
                stream=name,
                width=width,
                height=height,
                stats_every=args.stats_every,
            )
            app.add_flow(receiver, decoder, {("output", "input")})
            app.add_flow(receiver, stats, {("output", "input")})
            app.add_flow(decoder, visualizer, {("output", "receivers")})
            app.streams.append((stats, camera, args.frame_limit))

    app = D555Application(cuda_context, compose)
    # Each receiver blocks while it waits for its next frame; the other operators share the
    # remaining threads.
    app.scheduler(
        holoscan.schedulers.EventBasedScheduler(app, name="scheduler", worker_thread_number=4)
    )
    return run(app, streams["depth"][0].hololink(), device)


if __name__ == "__main__":
    sys.exit(main())
