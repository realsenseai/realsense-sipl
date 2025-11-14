# SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

import argparse
import ctypes
import logging
import os

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')

import holoscan
from cuda import cuda
# from models.dummy_cam import DummyCamera, DummyCameraMode
from dummy_realsense_d455 import DummyRealsenseCamD455, DummyRealsenseCameraMode
import hololink as hololink_module


class HoloscanApplication(holoscan.core.Application):
    def __init__(self, headless, fullscreen, cuda_context, cuda_device_ordinal, camera, camera_mode, frame_limit):
        logging.info("__init__")
        super().__init__()
        self._headless = headless
        self._fullscreen = fullscreen
        self._cuda_context = cuda_context
        self._cuda_device_ordinal = cuda_device_ordinal
        self._camera = camera
        self._camera_mode = camera_mode
        self._frame_limit = frame_limit

    def compose(self):
        logging.info("compose")

        if self._frame_limit:
            condition = holoscan.conditions.CountCondition(self, name="count", count=self._frame_limit)
        else:
            condition = holoscan.conditions.BooleanCondition(self, name="ok", enable_tick=True)

        self._camera.set_mode(self._camera_mode)

        allocator = holoscan.resources.UnboundedAllocator(self)

        dual_source = hololink_module.operators.D455RealSenseDualSourceOp(
            self,
            allocator=allocator,
            name="d455_realsense_dual_source"
        )

        visualizer_rgb = holoscan.operators.HolovizOp(
            self,
            name="holoviz_rgb",
            fullscreen=False,
            headless=self._headless,
            framebuffer_srgb=False,
            tensors=[{"name": "rgb_output", "type": "color"}]
        )

        visualizer_depth = holoscan.operators.HolovizOp(
            self,
            name="holoviz_depth",
            fullscreen=False,
            headless=self._headless,
            framebuffer_srgb=False,
            tensors=[{"name": "depth_output", "type": "color"}]
        )

        self.add_flow(dual_source, visualizer_rgb, {("rgb_output", "receivers")})
        self.add_flow(dual_source, visualizer_depth, {("depth_output", "receivers")})



def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--headless", action="store_true", help="Run in headless mode")
    parser.add_argument("--fullscreen", action="store_true", help="Run in fullscreen mode")
    parser.add_argument("--frame-limit", type=int, default=None, help="Exit after receiving this many frames")

    default_config = os.path.join(os.path.dirname(__file__), "example_configuration.yaml")
    parser.add_argument("--configuration", default=default_config, help="Configuration file")
    parser.add_argument("--hololink", default="192.168.0.2", help="IP address of Hololink board")
    parser.add_argument("--log-level", type=int, default=20, help="Logging level to display")
    parser.add_argument("--expander-configuration", type=int, default=0, choices=(0, 1), help="I2C Expander config")
    parser.add_argument("--pattern", type=int, choices=range(12), help="Configure to display a test pattern.")

    args = parser.parse_args()
    hololink_module.logging_level(args.log_level)
    logging.info("Initializing.")

    (cu_result,) = cuda.cuInit(0)
    assert cu_result == cuda.CUresult.CUDA_SUCCESS
    cu_device_ordinal = 0
    cu_result, cu_device = cuda.cuDeviceGet(cu_device_ordinal)
    assert cu_result == cuda.CUresult.CUDA_SUCCESS
    cu_result, cu_context = cuda.cuDevicePrimaryCtxRetain(cu_device)
    assert cu_result == cuda.CUresult.CUDA_SUCCESS

    camera = DummyRealsenseCamD455()
    camera_mode = DummyRealsenseCameraMode(0)

    app = HoloscanApplication(
        args.headless,
        args.fullscreen,
        cu_context,
        cu_device_ordinal,
        camera,
        camera_mode,
        args.frame_limit,
    )
    app.config(args.configuration)
    camera.setup_clock()
    camera.configure(camera_mode)
    camera.set_digital_gain_reg(0x4)
    os.environ["GXF_MEMORY_DEBUG"] = "1"
    app.run()

    (cu_result,) = cuda.cuDevicePrimaryCtxRelease(cu_device)
    assert cu_result == cuda.CUresult.CUDA_SUCCESS


if __name__ == "__main__":
    main()
