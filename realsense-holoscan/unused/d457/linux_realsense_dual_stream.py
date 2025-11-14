# SPDX-FileCopystream2Text: Copystream2 (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All stream2s reserved.
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

# See README.md for detailed information.

import argparse
import ctypes
import logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
import os
import sys
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))

import holoscan
from cuda import cuda
from realsense_d457 import RealsenseCamD457
import d457_registers

import hololink as hololink_module

from mipi_config import program_mipi_phy


class HoloscanApplication(holoscan.core.Application):
    def __init__(
        self,
        headless,
        fullscreen,
        cuda_context,
        cuda_device_ordinal,
        hololink_channel_stream1,
        camera_stream1,
        hololink_channel_stream2,
        camera_stream2,
        camera_mode,
        frame_limit,
    ):
        logging.info("__init__")
        super().__init__()
        self._headless = headless
        self._fullscreen = fullscreen
        self._cuda_context = cuda_context
        self._cuda_device_ordinal = cuda_device_ordinal
        self._hololink_channel_stream1 = hololink_channel_stream1
        self._camera_stream1 = camera_stream1
        self._hololink_channel_stream2 = hololink_channel_stream2
        self._camera_stream2 = camera_stream2
        self._camera_mode = camera_mode
        self._frame_limit = frame_limit

    def compose(self):
        logging.info("compose")
        if self._frame_limit:
            self._count_stream1 = holoscan.conditions.CountCondition(
                self,
                name="count_stream1",
                count=self._frame_limit,
            )
            condition_stream1 = self._count_stream1
            self._count_stream2 = holoscan.conditions.CountCondition(
                self,
                name="count_stream2",
                count=self._frame_limit,
            )
            condition_stream2 = self._count_stream2
        else:
            self._ok_stream1 = holoscan.conditions.BooleanCondition(
                self, name="ok_stream1", enable_tick=True
            )
            condition_stream1 = self._ok_stream1
            self._ok_stream2 = holoscan.conditions.BooleanCondition(
                self, name="ok_stream2", enable_tick=True
            )
            condition_stream2 = self._ok_stream2
        self._camera_stream1.set_mode(self._camera_mode)
        self._camera_stream2.set_mode(d457_registers.RealSense_Mode.REALSENSE_MODE_RGB_640x480_30FPS)

        # image_decoder_allocator_pool = holoscan.resources.BlockMemoryPool(
        #     self,
        #     name="pool",
        #     # storage_type of 1 is device memory
        #     storage_type=1,
        #     block_size=self._camera_stream1._width
        #     * ctypes.sizeof(ctypes.c_uint16) * 2
        #     * self._camera_stream1._height,
        #     num_blocks=2,
        # )

        image_decoder_allocator_pool = holoscan.resources.UnboundedAllocator(self)

        
        image_decoder_stream1 = hololink_module.operators.ImageDecoderOp(
            self,
            name="image_decoder_stream1",
            out_tensor_name="output",
            allocator=image_decoder_allocator_pool,
            cuda_device_ordinal=self._cuda_device_ordinal,
        )
        self._camera_stream1.configure_converter(image_decoder_stream1)

        image_decoder_stream2 = hololink_module.operators.ImageDecoderOp(
            self,
            name="image_decoder_stream2",
            out_tensor_name="output",
            allocator=image_decoder_allocator_pool,
            cuda_device_ordinal=self._cuda_device_ordinal,
        )
        self._camera_stream2.configure_converter(image_decoder_stream2)

        frame_size = image_decoder_stream1.get_csi_length()
        frame_context = self._cuda_context

        receiver_operator_stream1 = hololink_module.operators.LinuxReceiverOperator(
            self,
            condition_stream1,
            name="receiver_stream1",
            frame_size=frame_size,
            frame_context=frame_context,
            hololink_channel=self._hololink_channel_stream1,
            device=self._camera_stream1,
        )

        receiver_operator_stream2 = hololink_module.operators.LinuxReceiverOperator(
            self,
            condition_stream2,
            name="receiver_stream2",
            frame_size=frame_size,
            frame_context=frame_context,
            hololink_channel=self._hololink_channel_stream2,
            device=self._camera_stream2,
        )

        visualizer1 = holoscan.operators.HolovizOp(
            self,
            name="holoviz_stream1",
            fullscreen=self._fullscreen,
            headless=self._headless,
            framebuffer_srgb=False,
            window_title="Depth"
        )

        visualizer2 = holoscan.operators.HolovizOp(
            self,
            name="holoviz_stream2",
            fullscreen=self._fullscreen,
            headless=self._headless,
            framebuffer_srgb=False,
            window_title="RGB"
        )

        self.add_flow(receiver_operator_stream1, image_decoder_stream1, {("output", "input")})
        # self.add_flow(receiver_operator_stream2, image_decoder_stream2, {("output", "input")})
        self.add_flow(image_decoder_stream1, visualizer1, {("output", "receivers")})
        # self.add_flow(image_decoder_stream2, visualizer2, {("output", "receivers")})


def main():
    parser = argparse.ArgumentParser()
    modes = d457_registers.RealSense_Mode
    mode_choices = [mode.value for mode in modes]
    mode_help = " ".join([f"{mode.value}:{mode.name}" for mode in modes])
    parser.add_argument(
        "--camera-mode",
        type=int,
        choices=mode_choices,
        default=mode_choices[0],
        help=mode_help,
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
    default_configuration = os.path.join(
        os.path.dirname(__file__), "example_configuration.yaml"
    )
    parser.add_argument(
        "--configuration",
        default=default_configuration,
        help="Configuration file",
    )
    parser.add_argument(
        "--hololink",
        default="192.168.0.2",
        help="IP address of Hololink board",
    )
    parser.add_argument(
        "--log-level",
        type=int,
        default=20,
        help="Logging level to display",
    )
    parser.add_argument(
        "--expander-configuration",
        type=int,
        default=0,
        choices=(0, 1),
        help="I2C Expander configuration",
    )
    parser.add_argument(
        "--pattern",
        type=int,
        choices=range(12),
        help="Configure to display a test pattern.",
    )
    args = parser.parse_args()
    hololink_module.logging_level(args.log_level)
    logging.getLogger().setLevel(args.log_level)
    logging.info("Initializing.")
    # Get a handle to the GPU
    (cu_result,) = cuda.cuInit(0)
    assert cu_result == cuda.CUresult.CUDA_SUCCESS
    cu_device_ordinal = 0
    cu_result, cu_device = cuda.cuDeviceGet(cu_device_ordinal)
    assert cu_result == cuda.CUresult.CUDA_SUCCESS
    cu_result, cu_context = cuda.cuDevicePrimaryCtxRetain(cu_device)
    assert cu_result == cuda.CUresult.CUDA_SUCCESS

    # Get a handle to data sources.  First, find an enumeration packet
    # from the IP address we want to use.
    channel_metadata = hololink_module.Enumerator.find_channel(channel_ip=args.hololink)
    # Now make separate connection metadata for stream1 and stream2; and set them to
    # use sensor 0 and 1 respectively.  This will borrow the data plane
    # configuration we found on that interface.
    channel_metadata_stream1 = hololink_module.Metadata(channel_metadata)
    hololink_module.DataChannel.use_sensor(channel_metadata_stream1, 0)
    logging.info(f"{channel_metadata_stream1=}")

    channel_metadata_stream2 = hololink_module.Metadata(channel_metadata)
    hololink_module.DataChannel.use_sensor(channel_metadata_stream2, 1)
    logging.info(f"{channel_metadata_stream2=}")

     #
    hololink_channel_stream1 = hololink_module.DataChannel(channel_metadata_stream1)
    hololink_channel_stream2 = hololink_module.DataChannel(channel_metadata_stream2)
    # Get a handle to the camera
    camera_stream1 = RealsenseCamD457(hololink_channel_stream1)
    camera_stream2 = RealsenseCamD457(hololink_channel_stream2)

    logging.info("camera mode: %s", args.camera_mode)

    camera_mode = d457_registers.RealSense_Mode(
        args.camera_mode
    )

    # Set up the application
    application = HoloscanApplication(
        args.headless,
        args.fullscreen,
        cu_context,
        cu_device_ordinal,
        hololink_channel_stream1,
        camera_stream1,
        hololink_channel_stream2,
        camera_stream2,
        camera_mode,
        args.frame_limit,
    )
    application.config(args.configuration)
    # # Run it.
    hololink = hololink_channel_stream1.hololink()
    assert hololink is hololink_channel_stream2.hololink()
    hololink.start()
    hololink.reset()
    
    program_mipi_phy(0, 4, 1500, hololink)
    program_mipi_phy(1, 4, 1500, hololink)

    camera_stream2.setup_clock()
    camera_stream2.configure(camera_mode)
    camera_stream2.set_digital_gain_reg(0x4)
    camera_stream1.setup_clock()
    # camera_stream1.configure(camera_mode)
    camera_stream1.set_digital_gain_reg(0x4)

    os.environ["GXF_MEMORY_DEBUG"] = "1"
    application.run()
    hololink.stop()

    (cu_result,) = cuda.cuDevicePrimaryCtxRelease(cu_device)
    assert cu_result == cuda.CUresult.CUDA_SUCCESS


if __name__ == "__main__":
    main()
