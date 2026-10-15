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

"""Holoscan operators for D555 frames: GPU decoding for display and per-frame statistics."""

import logging
import time

import cupy as cp
import holoscan

from .profiles import BYTES_PER_PIXEL

_KERNELS = r"""
extern "C" {

// YUYV (Y0 U Y1 V per pixel pair) to packed RGB888, BT.601 limited range.
__global__ void yuyv_to_rgb(const unsigned char* in, unsigned char* out, int width, int height)
{
    const int x = blockIdx.x * blockDim.x + threadIdx.x;
    const int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= width || y >= height) {
        return;
    }
    const int pair = (y * width + (x & ~1)) * 2;
    const int luma = in[pair + ((x & 1) ? 2 : 0)] - 16;
    const int u = in[pair + 1] - 128;
    const int v = in[pair + 3] - 128;
    const int r = (298 * luma + 409 * v + 128) >> 8;
    const int g = (298 * luma - 100 * u - 208 * v + 128) >> 8;
    const int b = (298 * luma + 516 * u + 128) >> 8;
    const int o = (y * width + x) * 3;
    out[o + 0] = (unsigned char)min(max(r, 0), 255);
    out[o + 1] = (unsigned char)min(max(g, 0), 255);
    out[o + 2] = (unsigned char)min(max(b, 0), 255);
}

// Z16 depth to packed RGB888 through a piecewise-linear colormap over [min_m, max_m].
// Invalid depth (0) is drawn black.
__global__ void colorize_depth(const unsigned short* depth, unsigned char* out, int width,
                               int height, float depth_units, float min_m, float max_m,
                               const float* colormap, int colormap_size)
{
    const int x = blockIdx.x * blockDim.x + threadIdx.x;
    const int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= width || y >= height) {
        return;
    }
    const int i = y * width + x;
    const unsigned short d = depth[i];
    if (d == 0) {
        out[3 * i + 0] = 0;
        out[3 * i + 1] = 0;
        out[3 * i + 2] = 0;
        return;
    }
    float t = (d * depth_units - min_m) / (max_m - min_m);
    t = fminf(fmaxf(t, 0.0f), 1.0f) * (colormap_size - 1);
    const int lo = (int)t;
    const int hi = min(lo + 1, colormap_size - 1);
    const float f = t - lo;
    for (int c = 0; c < 3; ++c) {
        out[3 * i + c] = (unsigned char)(colormap[3 * lo + c] * (1.0f - f) + colormap[3 * hi + c] * f);
    }
}

}
"""

# Near to far: blue, cyan, yellow, red, dark red.
_DEPTH_COLORMAP = [
    (0.0, 0.0, 255.0),
    (0.0, 255.0, 255.0),
    (255.0, 255.0, 0.0),
    (255.0, 0.0, 0.0),
    (50.0, 0.0, 0.0),
]

_BLOCK = (16, 16, 1)


def _first_tensor(message):
    """The frame tensor of a receiver message (a one-entry tensor map)."""
    if hasattr(message, "values"):
        return next(iter(message.values()))
    return message


class D555DecoderOp(holoscan.core.Operator):
    """Turn one raw D555 frame into an RGB888 image for Holoviz or inference.

    Depth (Z16, millimeters) is colorized over [min_depth_m, max_depth_m]; RGB (YUYV) is
    converted to RGB. The output tensor has shape (height, width, 3) and type uint8.
    """

    def __init__(
        self,
        *args,
        stream,
        width,
        height,
        out_tensor_name="",
        min_depth_m=0.3,
        max_depth_m=4.0,
        **kwargs,
    ):
        if stream not in ("depth", "rgb"):
            raise ValueError(f"stream must be depth or rgb, not {stream!r}")
        self._stream = stream
        self._width = width
        self._height = height
        self._frame_size = width * height * BYTES_PER_PIXEL
        self._out_tensor_name = out_tensor_name
        self._min_depth_m = min_depth_m
        self._max_depth_m = max_depth_m
        self._short_frames = 0
        super().__init__(*args, **kwargs)

    def setup(self, spec):
        spec.input("input")
        spec.output("output")

    def start(self):
        module = cp.RawModule(code=_KERNELS)
        self._yuyv_to_rgb = module.get_function("yuyv_to_rgb")
        self._colorize_depth = module.get_function("colorize_depth")
        self._colormap = cp.asarray(_DEPTH_COLORMAP, dtype=cp.float32).ravel()
        self._grid = (
            (self._width + _BLOCK[0] - 1) // _BLOCK[0],
            (self._height + _BLOCK[1] - 1) // _BLOCK[1],
            1,
        )
        # Compile and launch the kernel once now. The receiver keeps only its newest frame, so a
        # slow first compute() would otherwise drop the stream's first frames.
        self._decode(cp.zeros(self._frame_size, dtype=cp.uint8))
        cp.cuda.get_current_stream().synchronize()

    def _decode(self, frame):
        """Launch the kernel for this stream on a full raw frame; returns the RGB image."""
        rgb = cp.empty((self._height, self._width, 3), dtype=cp.uint8)
        width, height = cp.int32(self._width), cp.int32(self._height)
        if self._stream == "depth":
            self._colorize_depth(
                self._grid,
                _BLOCK,
                (
                    frame.view(cp.uint16),
                    rgb,
                    width,
                    height,
                    cp.float32(0.001),
                    cp.float32(self._min_depth_m),
                    cp.float32(self._max_depth_m),
                    self._colormap,
                    cp.int32(len(_DEPTH_COLORMAP)),
                ),
            )
        else:
            self._yuyv_to_rgb(self._grid, _BLOCK, (frame, rgb, width, height))
        return rgb

    def compute(self, op_input, op_output, context):
        message = op_input.receive("input")
        frame = cp.asarray(_first_tensor(message)).ravel().view(cp.uint8)
        if frame.size < self._frame_size:
            # The receiver trims a frame to the bytes the camera reported writing.
            self._short_frames += 1
            if self._short_frames == 1 or self._short_frames % 100 == 0:
                logging.warning(
                    f"{self.name}: {self._short_frames} short frame(s), last {frame.size} of "
                    f"{self._frame_size} bytes; dropped"
                )
            return
        rgb = self._decode(frame[: self._frame_size])
        # The next operator may run on another CUDA stream; make the image complete first.
        cp.cuda.get_current_stream().synchronize()
        op_output.emit({self._out_tensor_name: rgb}, "output")


class FrameStatsOp(holoscan.core.Operator):
    """Count frames from one receiver and check their Hololink metadata.

    Reports the frame rate, gaps in the camera's frame counter, frames shorter than a full
    image, and frames the receiver itself dropped. result() gives a one-line summary and a
    pass/fail flag.

    The HSB receivers hold only the newest frame, so while the pipeline handles its first frame
    (Holoviz, for one, sets itself up then) the next one or two can be replaced before they are
    passed on; a packet capture shows the camera did send them. Gaps within the first
    warmup_frames are therefore reported as startup_gaps and do not fail the run.
    """

    def __init__(self, *args, stream, width, height, stats_every=150, warmup_frames=10, **kwargs):
        self._stream = stream
        self._frame_size = width * height * BYTES_PER_PIXEL
        self._stats_every = stats_every
        self._frames = 0
        self._first_time = None
        self._last_time = None
        self._max_interval = 0.0
        self._last_frame_number = None
        self._warmup_frames = warmup_frames
        self._frame_number_gaps = 0
        self._startup_gaps = 0
        self._short_frames = 0
        self._receiver_dropped = 0
        super().__init__(*args, **kwargs)

    def setup(self, spec):
        spec.input("input")

    def start(self):
        # Compile the CuPy kernels behind content() now rather than on the first frame.
        for _ in range(2):
            self.content(cp.random.randint(1, 256, self._frame_size, dtype=cp.uint8))

    def content(self, frame):
        """One-line summary of what a raw frame holds, to confirm the camera sends an image."""
        frame = frame[: self._frame_size]
        if self._stream == "depth":
            depth = frame.view(cp.uint16)
            valid = depth[depth > 0]
            median = int(cp.median(valid)) if valid.size else 0
            return f"valid_depth={valid.size / depth.size:.1%} median_depth_mm={median}"
        luma = frame[0::2].astype(cp.float32)
        return f"mean_luma={float(luma.mean()):.1f} luma_std={float(luma.std()):.1f}"

    def compute(self, op_input, op_output, context):
        message = op_input.receive("input")
        now = time.monotonic()
        warming_up = self._frames < self._warmup_frames
        if self._first_time is None:
            self._first_time = now
        elif not warming_up:
            self._max_interval = max(self._max_interval, now - self._last_time)
        self._last_time = now
        self._frames += 1

        metadata = self.metadata
        frame_number = metadata.get("frame_number")
        if frame_number is not None:
            # Hololink carries the low 16 bits of the camera's frame counter.
            expected = None if self._last_frame_number is None else (self._last_frame_number + 1)
            if expected is not None and frame_number != expected & 0xFFFF:
                if warming_up:
                    self._startup_gaps += 1
                else:
                    self._frame_number_gaps += 1
                logging.info(
                    f"{self._stream}: frame number {self._last_frame_number} -> {frame_number}"
                )
            self._last_frame_number = frame_number
        bytes_written = metadata.get("bytes_written")
        if bytes_written is not None and bytes_written < self._frame_size:
            self._short_frames += 1
        # RoceReceiverOp reports "dropped"; LinuxReceiverOperator reports "packets_dropped".
        self._receiver_dropped = max(
            self._receiver_dropped,
            metadata.get("dropped", 0) or 0,
            metadata.get("packets_dropped", 0) or 0,
        )

        if self._stats_every and self._frames % self._stats_every == 0:
            frame = cp.asarray(_first_tensor(message)).ravel().view(cp.uint8)
            logging.info(
                f"{self._stream}: {self._frames} frames, {self._fps():.2f} fps, "
                f"{self.content(frame)}"
            )

    def _fps(self):
        elapsed = (self._last_time - self._first_time) if self._first_time else 0.0
        return (self._frames - 1) / elapsed if elapsed > 0 else 0.0

    def result(self, frame_limit, stop_failed):
        """(summary line, passed). frame_limit counts receiver ticks, which also advance once a
        second while no frame arrives, so receiving fewer frames than that is a failure."""
        failures = [
            reason
            for reason, failed in (
                (f"frames<{frame_limit}", frame_limit and self._frames < frame_limit),
                ("gaps", self._frame_number_gaps),
                ("short", self._short_frames),
                ("dropped", self._receiver_dropped),
                ("stop", stop_failed),
            )
            if failed
        ]
        status = f"FAIL({','.join(failures)})" if failures else "PASS"
        elapsed = (self._last_time - self._first_time) if self._first_time else 0.0
        line = (
            f"RESULT stream={self._stream} frames={self._frames} fps={self._fps():.2f} "
            f"seconds={elapsed:.1f} frame_number_gaps={self._frame_number_gaps} "
            f"startup_gaps={self._startup_gaps} "
            f"short_frames={self._short_frames} receiver_dropped={self._receiver_dropped} "
            f"max_interval_ms={self._max_interval * 1000:.1f} status={status}"
        )
        return line, not failures
