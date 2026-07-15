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
# Holoscan player for the RealSense D457 over GMSL via NvSIPL. Uses the same NvSIPL query DB + UDDF
# driver as the reference `nvsipl_camera` client; each stream (depth Z16 / RGB YUYV / IR Y8I) is
# delivered as RAW16 and converted to RGBA on the GPU for display.
#
# Example (single-sensor query installed via tests/lib/common.sh `gen_query depth`):
#   sudo env LD_LIBRARY_PATH=/home/mic-742/sipl_libs D457_STREAM=depth \
#       python3 d457_sipl_player.py --camera-config D457_Camera --stream depth --headless \
#       --frame-limit 100

import argparse
import logging
import os

import cupy as cp
import holoscan
import numpy as np

import hololink as hololink_module


class D457StreamConvertOp(holoscan.core.Operator):
    """Convert one D457 RAW16 pipeline buffer to an RGBA uint8 image for Holoviz.

    The capture op emits a flat uint8 tensor (size bytes) keyed by `tensor_name`. We reshape to
    [height, bytes_per_line] (honoring the buffer row pitch), slice to width*2 bytes/row, then
    interpret per stream.
    """

    def __init__(
        self, *args, tensor_name, stream, width, height, bytes_per_line, depth_range_mm=4000,
        depth_colormap="jet", **kwargs
    ):
        super().__init__(*args, **kwargs)
        self._tensor_name = tensor_name
        self._stream = stream
        self._width = width
        self._height = height
        self._bytes_per_line = bytes_per_line
        self._depth_range_mm = depth_range_mm
        self._depth_colormap = depth_colormap
        self._jet = self._make_jet_lut()

    @staticmethod
    def _make_jet_lut():
        # 256-entry Jet colormap [256, 3] uint8 (like the RealSense viewer's default depth coloring).
        x = np.linspace(0.0, 1.0, 256)
        r = np.clip(np.minimum(4 * x - 1.5, -4 * x + 4.5), 0, 1)
        g = np.clip(np.minimum(4 * x - 0.5, -4 * x + 3.5), 0, 1)
        b = np.clip(np.minimum(4 * x + 0.5, -4 * x + 2.5), 0, 1)
        return cp.asarray((np.stack([r, g, b], axis=1) * 255).astype(np.uint8))

    def setup(self, spec):
        spec.input("input")
        spec.output("output")

    def compute(self, op_input, op_output, context):
        in_message = op_input.receive("input")
        flat = cp.asarray(in_message.get(self._tensor_name)).reshape(-1).view(cp.uint8)

        # Reshape to [height, bytes_per_line] and drop any row padding -> [height, width*2].
        rowbytes = self._width * 2
        frame = flat[: self._height * self._bytes_per_line].reshape(
            self._height, self._bytes_per_line
        )[:, :rowbytes]

        if self._stream == "depth":
            # Z16 millimeters. The SIPL RAW16 buffer delivers depth BIG-ENDIAN (high byte first);
            # reading it little-endian sawtooths a smooth surface into cyclical bands. Reassemble
            # as (high << 8) | low.
            b = frame.reshape(self._height, self._width, 2).astype(cp.uint16)
            z = (b[:, :, 0] << 8) | b[:, :, 1]
            if self._depth_colormap == "gray":
                g = cp.clip(z.astype(cp.float32) / float(self._depth_range_mm) * 255.0, 0, 255).astype(cp.uint8)
                rgba = self._gray_to_rgba(g)
            else:
                # Histogram-equalized Jet colormap, like the RealSense viewer (invalid/0 depth -> black).
                rgba = self._depth_to_jet_rgba(z)
        elif self._stream == "rgb":
            # YUYV [Y0, U, Y1, V] -> RGB.
            rgba = self._yuyv_to_rgba(frame.astype(cp.float32))
        else:  # ir: interleaved left/right Y8; show the left view.
            g = frame[:, 0::2]
            rgba = self._gray_to_rgba(g)

        op_output.emit({self._tensor_name: rgba}, "output")

    def _depth_to_jet_rgba(self, z):
        # Histogram-equalize valid (non-zero) depth across 0..255, then apply the Jet colormap.
        h, w = z.shape
        zf = z.reshape(-1)
        hist = cp.bincount(zf, minlength=65536).astype(cp.float32)
        hist[0] = 0.0  # ignore invalid / zero depth
        cdf = cp.cumsum(hist)
        lut = (cdf / cp.maximum(cdf[-1], 1.0) * 255.0).astype(cp.uint8)  # [65536] equalization LUT
        eq = lut[zf].reshape(h, w)
        rgb = self._jet[eq]  # [H, W, 3]
        rgb[z == 0] = 0  # invalid depth -> black
        rgba = cp.empty((h, w, 4), cp.uint8)
        rgba[:, :, 0:3] = rgb
        rgba[:, :, 3] = 255
        return rgba

    @staticmethod
    def _gray_to_rgba(g):
        h, w = g.shape
        rgba = cp.empty((h, w, 4), cp.uint8)
        rgba[:, :, 0] = g
        rgba[:, :, 1] = g
        rgba[:, :, 2] = g
        rgba[:, :, 3] = 255
        return rgba

    def _yuyv_to_rgba(self, b):
        Y = b[:, 0::2]
        U = cp.repeat(b[:, 1::4], 2, axis=1) - 128.0
        V = cp.repeat(b[:, 3::4], 2, axis=1) - 128.0
        R = cp.clip(Y + 1.402 * V, 0, 255)
        G = cp.clip(Y - 0.344 * U - 0.714 * V, 0, 255)
        B = cp.clip(Y + 1.772 * U, 0, 255)
        h, w = Y.shape
        rgba = cp.empty((h, w, 4), cp.uint8)
        rgba[:, :, 0] = R.astype(cp.uint8)
        rgba[:, :, 1] = G.astype(cp.uint8)
        rgba[:, :, 2] = B.astype(cp.uint8)
        rgba[:, :, 3] = 255
        return rgba


class HoloscanApplication(holoscan.core.Application):
    def __init__(self, camera_config, json_config, stream, headless, fullscreen, frame_limit,
                 depth_colormap="jet"):
        super().__init__()
        self._camera_config = camera_config
        self._json_config = json_config
        self._stream = stream
        self._headless = headless
        self._fullscreen = fullscreen
        self._frame_limit = frame_limit
        self._depth_colormap = depth_colormap

    def compose(self):
        if self._frame_limit:
            condition = holoscan.conditions.CountCondition(
                self, name="count", count=self._frame_limit
            )
        else:
            condition = holoscan.conditions.BooleanCondition(
                self, name="ok", enable_tick=True
            )

        sipl_capture = hololink_module.operators.D457SIPLCaptureOp(
            self,
            condition,
            name="d457_sipl_capture",
            camera_config=self._camera_config,
            json_config=self._json_config,
            stream=self._stream,
        )
        camera_info = sipl_capture.get_camera_info()

        # Lay the streams out in a grid (1 wide for a single stream, else 2 columns) so 3+ streams
        # form a compact window instead of one ultra-wide row that scrolls off a VNC viewer.
        n = len(camera_info)
        cols = 1 if n == 1 else 2
        rows = (n + cols - 1) // cols
        cell_w = 1.0 / cols
        cell_h = 1.0 / rows
        specs = []
        for i, info in enumerate(camera_info):
            r, c = divmod(i, cols)
            view = holoscan.operators.HolovizOp.InputSpec.View()
            view.offset_x = c * cell_w
            view.offset_y = r * cell_h
            view.width = cell_w
            view.height = cell_h
            spec = holoscan.operators.HolovizOp.InputSpec(
                info.output_name, holoscan.operators.HolovizOp.InputType.COLOR
            )
            spec.views = [view]
            specs.append(spec)

        # Size the window to the grid at native 16:9 per cell (each 1280x720), so nothing is
        # stretched. (Fullscreen isn't usable over VNC -- GLFW's RANDR mode-set fails.)
        window_width = cols * max(info.width for info in camera_info)
        window_height = rows * max(info.height for info in camera_info)
        visualizer = holoscan.operators.HolovizOp(
            self,
            name="holoviz",
            width=window_width,
            height=window_height,
            headless=self._headless,
            tensors=specs,
        )

        for info in camera_info:
            convert = D457StreamConvertOp(
                self,
                name=f"convert_{info.output_name}",
                tensor_name=info.output_name,
                stream=info.stream,
                width=info.width,
                height=info.height,
                bytes_per_line=info.bytes_per_line,
                depth_colormap=self._depth_colormap,
            )
            self.add_flow(sipl_capture, convert, {("output", "input")})
            self.add_flow(convert, visualizer, {("output", "receivers")})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--list-configs", action="store_true", help="List available configs then exit"
    )
    parser.add_argument(
        "--camera-config", default="", help="Camera config to use (e.g. D457_Camera)"
    )
    parser.add_argument(
        "--json-config", default="", help="JSON configuration file to use"
    )
    parser.add_argument(
        "--stream",
        default="",
        choices=("", "depth", "rgb", "ir"),
        help="Stream to select for a single-sensor config (sets D457_STREAM before init)",
    )
    parser.add_argument("--headless", action="store_true", help="Run in headless mode")
    parser.add_argument(
        "--fullscreen", action="store_true", help="Run in fullscreen mode"
    )
    parser.add_argument(
        "--frame-limit",
        type=int,
        default=None,
        help="Exit after receiving this many frames",
    )
    parser.add_argument(
        "--depth-colormap",
        default="jet",
        choices=("jet", "gray"),
        help="Depth rendering: histogram-equalized Jet (like the RealSense viewer) or plain grayscale",
    )
    parser.add_argument(
        "--log-level", type=int, default=20, help="Logging level to display"
    )
    args = parser.parse_args()
    hololink_module.logging_level(args.log_level)

    if args.list_configs:
        hololink_module.operators.D457SIPLCaptureOp.list_available_configs(args.json_config)
        return

    # Keep the env in sync so the driver selects the requested stream even if --stream is unset but
    # D457_STREAM is exported by the caller.
    if args.stream:
        os.environ["D457_STREAM"] = args.stream

    application = HoloscanApplication(
        args.camera_config,
        args.json_config,
        args.stream,
        args.headless,
        args.fullscreen,
        args.frame_limit,
        args.depth_colormap,
    )
    application.run()


if __name__ == "__main__":
    main()
