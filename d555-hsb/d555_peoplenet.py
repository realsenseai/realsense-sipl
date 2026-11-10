# SPDX-FileCopyrightText: Copyright (c) 2026, RealSense AI. All rights reserved.
# SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

"""Detect people and faces in the RealSense D555 RGB stream with TAO PeopleNet.

The graph follows Holoscan Sensor Bridge's linux_tao_peoplenet.py example and reuses its
pre- and post-processing operators from the HSB examples directory.

    python3 d555_peoplenet.py
    python3 d555_peoplenet.py --receiver roce --profile 6
"""

import argparse
import os
import sys

import holoscan
from d555.app import D555Application, add_common_arguments, run, setup_logging, validate_profiles
from d555.camera import find_camera, open_stream
from d555.operators import D555DecoderOp, FrameStatsOp
from d555.profiles import DEFAULT_PROFILE, STREAMS
from d555.receivers import frame_condition, init_cuda, make_receiver

STREAM = "rgb"


def hsb_examples_directory():
    """HSB's examples directory: $HSB_ROOT/examples, else ./examples (docker/demo.sh runs from
    the HSB checkout)."""
    return os.path.join(os.environ.get("HSB_ROOT", os.getcwd()), "examples")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--profile", type=int, default=None, help="RGB profile index (default: 640x360 at 30 fps)"
    )
    parser.add_argument(
        "--configuration",
        default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "d555_peoplenet.yaml"),
        help="Holoscan configuration file",
    )
    parser.add_argument(
        "--engine",
        default=os.path.join(hsb_examples_directory(), "resnet34_peoplenet_int8.onnx"),
        help="PeopleNet ONNX model (a TensorRT engine is built from it on the first run)",
    )
    add_common_arguments(parser)
    args = parser.parse_args()
    setup_logging(args)

    sys.path.append(hsb_examples_directory())
    try:
        from tao_peoplenet import FormatInferenceInputOp, PostprocessorOp
    except ImportError:
        parser.error(
            f"cannot import tao_peoplenet from {hsb_examples_directory()}; run from the "
            "Holoscan Sensor Bridge checkout or set HSB_ROOT"
        )
    if not os.path.exists(args.engine):
        parser.error(f"{args.engine} not found; see the README for downloading PeopleNet")

    profile = DEFAULT_PROFILE[STREAM] if args.profile is None else args.profile
    validate_profiles(parser, {STREAM: profile}, args.allow_overload)
    width, height, _ = STREAMS[STREAM].profiles[profile]

    channel, camera = open_stream(find_camera(args.hololink), STREAM, profile)
    device, cuda_context = init_cuda(args.gpu)

    def compose(app):
        condition = frame_condition(app, STREAM, args.frame_limit)
        receiver = make_receiver(app, args, STREAM, profile, channel, camera, condition)
        # The preprocessor and the Holoviz configuration expect the image as the unnamed tensor.
        decoder = D555DecoderOp(
            app, name="decoder", stream=STREAM, width=width, height=height, out_tensor_name=""
        )
        stats = FrameStatsOp(
            app,
            name="stats",
            stream=STREAM,
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
            window_title="D555 PeopleNet",
            **app.kwargs("holoviz"),
        )
        visualizer.metadata_policy = holoscan.core.MetadataPolicy.UPDATE

        pool = holoscan.resources.UnboundedAllocator(app)
        preprocessor_args = app.kwargs("preprocessor")
        preprocessor = holoscan.operators.FormatConverterOp(
            app, name="preprocessor", pool=pool, **preprocessor_args
        )
        format_input = FormatInferenceInputOp(app, name="transpose", pool=pool)
        inference = holoscan.operators.InferenceOp(
            app,
            name="inference",
            allocator=pool,
            model_path_map={"face_detect": args.engine},
            **app.kwargs("inference"),
        )
        postprocessor_args = app.kwargs("postprocessor")
        postprocessor_args["image_width"] = preprocessor_args["resize_width"]
        postprocessor_args["image_height"] = preprocessor_args["resize_height"]
        postprocessor = PostprocessorOp(
            app, name="postprocessor", allocator=pool, **postprocessor_args
        )

        app.add_flow(receiver, decoder, {("output", "input")})
        app.add_flow(receiver, stats, {("output", "input")})
        app.add_flow(decoder, visualizer, {("output", "receivers")})
        app.add_flow(decoder, preprocessor, {("output", "")})
        app.add_flow(preprocessor, format_input)
        app.add_flow(format_input, inference, {("", "receivers")})
        app.add_flow(inference, postprocessor, {("transmitter", "in")})
        app.add_flow(postprocessor, visualizer, {("out", "receivers")})
        app.streams.append((stats, camera, args.frame_limit))

    app = D555Application(cuda_context, compose)
    app.config(args.configuration)
    return run(app, channel.hololink(), device)


if __name__ == "__main__":
    sys.exit(main())
