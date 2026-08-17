# SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
#
# TAO PeopleNet (person/face detection) on the RealSense D457 over GMSL via NvSIPL.
#
# Pipeline:
#   d457_sipl_capture (RAW16) -> D457ToRgb888Op (CuPy) -> +- HolovizOp (color image)
#                                                         +- preprocessor -> transpose
#                                                            -> InferenceOp -> postprocessor
#                                                            -> HolovizOp (person/face boxes)
#
# Detection is RGB-native, so the default stream is `rgb`. Reuses the PeopleNet inference/
# postprocessor ops + config from tao_peoplenet.py / tao_peoplenet.yaml.
#
# Prereq: the PeopleNet ONNX next to this file (examples/resnet34_peoplenet_int8.onnx); a
# single-sensor query for the chosen stream (tests/lib/common.sh `gen_query rgb`). Example:
#   sudo env LD_LIBRARY_PATH=/home/mic-742/sipl_libs D457_STREAM=rgb \
#       python3 d457_tao_peoplenet.py --camera-config D457_Camera --stream rgb

import argparse
import os

import cupy as cp
import holoscan
import numpy as np
from tao_peoplenet import FormatInferenceInputOp, PostprocessorOp

import hololink as hololink_module


class D457ConvertOp(holoscan.core.Operator):
    """Convert one D457 RAW16 pipeline buffer for both display and inference.

    Emits two tensors:
      - ""    : RGBA [H, W, 4] uint8 for the Holoviz color layer (Holoviz's CUDA->Vulkan import
                needs a 4-channel image here, matching the working d457_sipl_player path).
      - "rgb" : RGB888 [H, W, 3] uint8 for the PeopleNet preprocessor (the model wants 3 channels).
    """

    def __init__(self, *args, tensor_name, stream, width, height, bytes_per_line, **kwargs):
        self._tensor_name = tensor_name
        self._stream = stream
        self._width = width
        self._height = height
        self._bytes_per_line = bytes_per_line
        super().__init__(*args, **kwargs)

    def setup(self, spec):
        spec.input("input")
        spec.output("output")

    def compute(self, op_input, op_output, context):
        in_message = op_input.receive("input")
        flat = cp.asarray(in_message.get(self._tensor_name)).reshape(-1).view(cp.uint8)
        rowbytes = self._width * 2
        frame = flat[: self._height * self._bytes_per_line].reshape(
            self._height, self._bytes_per_line
        )[:, :rowbytes]

        if self._stream == "rgb":
            b = frame.astype(cp.float32)
            Y = b[:, 0::2]
            U = cp.repeat(b[:, 1::4], 2, axis=1) - 128.0
            V = cp.repeat(b[:, 3::4], 2, axis=1) - 128.0
            R = cp.clip(Y + 1.402 * V, 0, 255).astype(cp.uint8)
            G = cp.clip(Y - 0.344 * U - 0.714 * V, 0, 255).astype(cp.uint8)
            B = cp.clip(Y + 1.772 * U, 0, 255).astype(cp.uint8)
        else:
            # depth (Z16 mm) / ir (left Y8): grayscale replicated into RGB.
            if self._stream == "depth":
                # Z16 is big-endian in the SIPL buffer (high byte first) -- see d457_sipl_player.py.
                bb = frame.reshape(self._height, self._width, 2).astype(cp.uint16)
                z = ((bb[:, :, 0] << 8) | bb[:, :, 1]).astype(cp.float32)
                g = cp.clip(z / 4000.0 * 255.0, 0, 255).astype(cp.uint8)
            else:  # ir: interleaved L/R Y8, take the left view
                g = frame[:, 0::2]
            R = G = B = g

        h, w = R.shape
        rgb = cp.empty((h, w, 3), cp.uint8)
        rgb[:, :, 0] = R
        rgb[:, :, 1] = G
        rgb[:, :, 2] = B
        rgba = cp.empty((h, w, 4), cp.uint8)
        rgba[:, :, 0] = R
        rgba[:, :, 1] = G
        rgba[:, :, 2] = B
        rgba[:, :, 3] = 255

        op_output.emit({"image": rgba, "rgb": rgb}, "output")


class DrawBoxesOp(holoscan.core.Operator):
    """Draw the PeopleNet person/face boxes onto the RGBA image, emit a single annotated image.

    Holoviz's geometry (rectangles) layer fails to import its small coordinate buffer on this
    Thor/SIPL path (CUDA_ERROR_INVALID_VALUE), while the color image layer works. So we burn the
    boxes into the image here and feed Holoviz one color layer. The box coords from PostprocessorOp
    are normalized [0,1], so they map directly onto the full-resolution image.

    Inputs: "image" (RGBA [H,W,4], every frame) and "boxes" (person/faces, lagging by the inference
    pipeline depth). The overlay therefore trails the image by a few frames -- fine for a live demo.
    """

    # (R,G,B) per class.
    COLORS = {"person": (0, 255, 0), "faces": (255, 0, 0)}

    def __init__(self, *args, line_width=3, **kwargs):
        self._t = line_width
        self._n = 0
        super().__init__(*args, **kwargs)

    def setup(self, spec):
        spec.input("image")
        spec.input("boxes")
        spec.output("output")

    def _draw(self, rgba, x0, y0, x1, y1, color):
        h, w = rgba.shape[:2]
        x0 = max(0, min(w - 1, int(x0)))
        x1 = max(0, min(w, int(x1)))
        y0 = max(0, min(h - 1, int(y0)))
        y1 = max(0, min(h, int(y1)))
        if x1 - x0 < 2 or y1 - y0 < 2:
            return
        t = self._t
        c = cp.asarray(color + (255,), dtype=cp.uint8)
        rgba[y0:y1, x0 : x0 + t] = c
        rgba[y0:y1, x1 - t : x1] = c
        rgba[y0 : y0 + t, x0:x1] = c
        rgba[y1 - t : y1, x0:x1] = c

    def compute(self, op_input, op_output, context):
        image_message = op_input.receive("image")
        box_message = op_input.receive("boxes")
        rgba = cp.ascontiguousarray(cp.asarray(image_message.get("image")))
        h, w = rgba.shape[:2]
        counts = {}
        for name, color in self.COLORS.items():
            t = box_message.get(name)
            if t is None:
                continue
            pts = np.asarray(t).reshape(-1, 2)  # normalized (x, y) pairs
            rects = pts.reshape(-1, 2, 2)  # each rect = (xmin,ymin),(xmax,ymax)
            n = 0
            for (x0, y0), (x1, y1) in rects:
                if (x1 - x0) >= 0.003 and (y1 - y0) >= 0.003:  # skip the empty/zero placeholder box
                    self._draw(rgba, x0 * w, y0 * h, x1 * w, y1 * h, color)
                    n += 1
            counts[name] = n
        self._n += 1
        if self._n <= 3 or self._n % 30 == 0:
            print(f"[detect frame {self._n}] {counts}", flush=True)
        op_output.emit({"image": rgba}, "output")


class HoloscanApplication(holoscan.core.Application):
    def __init__(self, camera_config, json_config, stream, headless, fullscreen, frame_limit, engine):
        super().__init__()
        self._camera_config = camera_config
        self._json_config = json_config
        self._stream = stream
        self._headless = headless
        self._fullscreen = fullscreen
        self._frame_limit = frame_limit
        self._engine = engine

    def compose(self):
        if self._frame_limit:
            condition = holoscan.conditions.CountCondition(self, name="count", count=self._frame_limit)
        else:
            condition = holoscan.conditions.BooleanCondition(self, name="ok", enable_tick=True)

        sipl_capture = hololink_module.operators.D457SIPLCaptureOp(
            self,
            condition,
            name="d457_sipl_capture",
            camera_config=self._camera_config,
            json_config=self._json_config,
            stream=self._stream,
        )
        camera_info = sipl_capture.get_camera_info()
        assert len(camera_info) == 1, "d457_tao_peoplenet supports a single-sensor config"
        info = camera_info[0]

        convert = D457ConvertOp(
            self,
            name="convert",
            tensor_name=info.output_name,
            stream=info.stream,
            width=info.width,
            height=info.height,
            bytes_per_line=info.bytes_per_line,
        )

        # Holoviz renders a single color layer (the boxes are burned into the image by DrawBoxesOp;
        # Holoviz's geometry layer fails its CUDA buffer import on this Thor/SIPL path). Window sized
        # to the image so it isn't stretched (fullscreen isn't usable over VNC).
        visualizer = holoscan.operators.HolovizOp(
            self,
            name="holoviz",
            width=info.width,
            height=info.height,
            headless=self._headless,
            tensors=[{"name": "image", "type": "color", "opacity": 1.0, "priority": 0}],
        )

        pool = holoscan.resources.UnboundedAllocator(self)
        preprocessor_args = self.kwargs("preprocessor")
        preprocessor_args["in_tensor_name"] = "rgb"  # the 3-channel tensor for the model
        preprocessor = holoscan.operators.FormatConverterOp(
            self, name="preprocessor", pool=pool, **preprocessor_args
        )
        format_input = FormatInferenceInputOp(self, name="transpose", pool=pool)
        inference = holoscan.operators.InferenceOp(
            self,
            name="inference",
            allocator=pool,
            model_path_map={"face_detect": self._engine},
            **self.kwargs("inference"),
        )
        postprocessor_args = self.kwargs("postprocessor")
        postprocessor_args["image_width"] = preprocessor_args["resize_width"]
        postprocessor_args["image_height"] = preprocessor_args["resize_height"]
        postprocessor = PostprocessorOp(
            self, name="postprocessor", allocator=pool, **postprocessor_args
        )
        draw_boxes = DrawBoxesOp(self, name="draw_boxes")

        # capture -> convert -> draw_boxes(image) ; convert -> inference branch -> draw_boxes(boxes)
        # -> Holoviz. DrawBoxesOp burns the detections into the image and emits one color layer.
        self.add_flow(sipl_capture, convert, {("output", "input")})
        self.add_flow(convert, draw_boxes, {("output", "image")})
        self.add_flow(convert, preprocessor, {("output", "")})
        self.add_flow(preprocessor, format_input)
        self.add_flow(format_input, inference, {("", "receivers")})
        self.add_flow(inference, postprocessor, {("transmitter", "in")})
        self.add_flow(postprocessor, draw_boxes, {("out", "boxes")})
        self.add_flow(draw_boxes, visualizer, {("output", "receivers")})

        # Image and postprocessor both feed Holoviz; we don't use the metadata.
        self.enable_metadata(False)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--list-configs", action="store_true", help="List available configs then exit")
    parser.add_argument("--camera-config", default="", help="Camera config to use (e.g. D457_Camera)")
    parser.add_argument("--json-config", default="", help="JSON configuration file to use")
    parser.add_argument(
        "--stream", default="rgb", choices=("depth", "rgb", "ir"),
        help="D457 stream to run detection on (PeopleNet is RGB-native; default rgb)",
    )
    parser.add_argument("--headless", action="store_true", help="Run in headless mode")
    parser.add_argument("--fullscreen", action="store_true", help="Run in fullscreen mode")
    parser.add_argument("--frame-limit", type=int, default=None, help="Exit after N frames")
    default_configuration = os.path.join(os.path.dirname(__file__), "tao_peoplenet.yaml")
    parser.add_argument("--configuration", default=default_configuration, help="Configuration file")
    default_engine = os.path.join(os.path.dirname(__file__), "resnet34_peoplenet_int8.onnx")
    parser.add_argument("--engine", default=default_engine, help="PeopleNet ONNX / TRT model")
    parser.add_argument("--log-level", type=int, default=20, help="Logging level to display")
    args = parser.parse_args()
    hololink_module.logging_level(args.log_level)

    if args.list_configs:
        hololink_module.operators.D457SIPLCaptureOp.list_available_configs(args.json_config)
        return

    if args.stream:
        os.environ["D457_STREAM"] = args.stream

    application = HoloscanApplication(
        args.camera_config,
        args.json_config,
        args.stream,
        args.headless,
        args.fullscreen,
        args.frame_limit,
        args.engine,
    )
    application.config(args.configuration)
    application.run()


if __name__ == "__main__":
    main()
