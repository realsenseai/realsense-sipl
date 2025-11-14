"""
SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
SPDX-License-Identifier: Apache-2.0
"""

import logging
import time

import hololink as hololink_module
from realsense_model import Endianness, DataWidth

from collections import OrderedDict

import d457_registers

# Camera info
DRIVER_NAME = "REALSENSE-D457"
VERSION = 1
DS5_START_MAX_COUNT = 10
DS5_START_POLL_TIME = 100  # ms


class RealsenseCamD457:
    def __init__(
        self,
        hololink_channel,
        i2c_controller_address=hololink_module.CAM_I2C_CTRL,
    ):
        self._hololink = hololink_channel.hololink()
        self._i2c = self._hololink.get_i2c(i2c_controller_address)

        # default values
        self._running = False
        self._pixel_format = hololink_module.operators.ImageDecoderOp.PixelFormat.Z16
        self._width = 640
        self._height = 480
        self._camera_start_registers = d457_registers.realsense_mode_depth_640x480_30fps
        self._camera_config_status_register = d457_registers.DS5_DEPTH_CONFIG_STATUS
        self._camera_stream_status_register = d457_registers.DS5_DEPTH_STREAM_STATUS
        self._mode = 0

    def setup_clock(self):
        # Optional: if RealSense needs a clock from Hololink
        self._hololink.setup_clock(
            hololink_module.renesas_bajoran_lite_ts1.device_configuration()
        )
        pass

    def set_mode(self, realsense_mode):
        logging.info(f"[Realsense Camera] Mode set to: {realsense_mode}")
        self._mode = realsense_mode
        if realsense_mode.value < len(d457_registers.RealSense_Mode):
            self._mode = realsense_mode
            mode = d457_registers.realsense_frame_format[self._mode.value]
            self._height = mode.height
            self._width = mode.width
            self._pixel_format = mode.pixel_format
            self._camera_start_registers = mode.start_registers
            self._camera_stop_registers = mode.stop_registers
            self._camera_config_status_register = mode.config_status_register
            self._camera_stream_status_register = mode.stream_status_register
        else:
            logging.error("Incorrect mode for Realsense D457 camera.")
            self._mode = -1

    def power_on(self):
        """Enable power to RealSense via I2C expander"""
        time.sleep(0.1)

    def configure(self, camera_mode):
        """Configure the camera (if needed) over I2C"""
        self.power_on()
        logging.info("Sending RealSense config over I2C...")

        # configure the camera based on the mode
        self.configure_camera(camera_mode)

    def start_camera_stream(self, registers, config_status_reg, stream_status_reg):
        self.apply_registers(
            registers,
            reg_size=DataWidth.BITS_16,
            val_size=DataWidth.BITS_16,
            endian=Endianness.LITTLE,
            delay_sec=0.1,
        )

        for attempt in range(DS5_START_MAX_COUNT):
            try:
                config_status = self.get_register(
                    d457_registers.MUX_I2C_ADDR,
                    config_status_reg,
                    reg_size=DataWidth.BITS_16,
                    val_size=DataWidth.BITS_16,
                    endian=Endianness.LITTLE,
                )
                stream_status = self.get_register(
                    d457_registers.MUX_I2C_ADDR,
                    stream_status_reg,
                    reg_size=DataWidth.BITS_16,
                    val_size=DataWidth.BITS_16,
                    endian=Endianness.LITTLE,
                )
                if (
                    config_status == d457_registers.DS5_STATUS_STREAMING and
                    stream_status == d457_registers.DS5_STREAM_STREAMING
                ):
                    logging.info("RealSenseCam: Streaming started successfully.")
                    return True
            except Exception as e:
                logging.error(f"Streaming check error: {e}")
            time.sleep(DS5_START_POLL_TIME / 1000.0)

        if attempt == DS5_START_MAX_COUNT - 1:
            # self.stop()  # Stop if we failed to start streaming
            logging.error(
                "RealSenseCam: Failed to start streaming after maximum attempts."
            )

        return False

    def stop_camera_stream(self, registers=d457_registers.REALSENSE_STOP_ALL_STREAM):
        """Stop Streaming"""
        logging.info("RealSenseCam: stop_camera_stream()")

        self.apply_registers(
            registers,
            reg_size=DataWidth.BITS_16,
            val_size=DataWidth.BITS_16,
            endian=Endianness.LITTLE,
        )

    def start(self):
        """Start Streaming"""
        logging.info("RealSenseCam: start()")

        #
        # Setting these register is time-consuming.
        logging.info(
            "RealSenseCam: Stopping any previous streaming before starting new one."
        )
        
        # If the camera stream is already running, stop it first
        self.stop_camera_stream(self._camera_stop_registers)

        logging.info("RealSenseCam: Attempting to start streaming...")

        self.start_camera_stream(
            self._camera_start_registers,
            self._camera_config_status_register,
            self._camera_stream_status_register,
        )

    def stop(self):
        logging.info("RealSenseCam: stop()")
        self.stop_camera_stream(self._camera_stop_registers)
        self._running = False

    def set_register(
        self,
        i2c_address,
        register,
        value,
        reg_size: DataWidth,
        val_size: DataWidth,
        endian: Endianness,
    ):
        logging.debug(
            f"WRITE >> i2c_address=0x{int(i2c_address):X} set_register(register=0x{int(register):04X}, value=0x{int(value):04X})"
        )

        write_bytes = bytearray(4)
        serializer = hololink_module.Serializer(write_bytes)

        if reg_size == DataWidth.BITS_32:
            if endian == Endianness.BIG:
                serializer.append_uint32_be(register)
            else:
                serializer.append_uint32_le(register)
        elif reg_size == DataWidth.BITS_16:
            if endian == Endianness.BIG:
                serializer.append_uint16_be(register)
            else:
                serializer.append_uint16_le(register)
        elif reg_size == DataWidth.BITS_8:
            serializer.append_uint8(register)

        if val_size == DataWidth.BITS_32:
            if endian == Endianness.BIG:
                serializer.append_uint32_be(value)
            else:
                serializer.append_uint32_le(value)
        elif val_size == DataWidth.BITS_16:
            if endian == Endianness.BIG:
                serializer.append_uint16_be(value)
            else:
                serializer.append_uint16_le(value)
        elif val_size == DataWidth.BITS_8:
            serializer.append_uint8(value)

        self._i2c.i2c_transaction(
            i2c_address, write_bytes[: serializer.length()], 0, timeout=None
        )

    def get_register(
        self,
        i2c_address,
        register,
        reg_size: DataWidth,
        val_size: DataWidth,
        endian: Endianness,
    ):
        write_bytes = bytearray(2)
        serializer = hololink_module.Serializer(write_bytes)

        if reg_size == DataWidth.BITS_32:
            if endian == Endianness.BIG:
                serializer.append_uint32_be(register)
            else:
                serializer.append_uint32_le(register)
        elif reg_size == DataWidth.BITS_16:
            if endian == Endianness.BIG:
                serializer.append_uint16_be(register)
            else:
                serializer.append_uint16_le(register)
        elif reg_size == DataWidth.BITS_8:
            serializer.append_uint8(register)

        read_byte_count = val_size.value
        reply = self._i2c.i2c_transaction(
            i2c_address,
            write_bytes[: serializer.length()],
            read_byte_count,
            timeout=None,
        )

        deserializer = hololink_module.Deserializer(reply)
        if val_size == DataWidth.BITS_32:
            result = (
                deserializer.next_uint32_be()
                if endian == Endianness.BIG
                else deserializer.next_uint32_le()
            )
        elif val_size == DataWidth.BITS_16:
            result = (
                deserializer.next_uint16_be()
                if endian == Endianness.BIG
                else deserializer.next_uint16_le()
            )
        elif val_size == DataWidth.BITS_8:
            result = deserializer.next_uint8()

        logging.debug(
            f"READ  >> i2c_address=0x{int(i2c_address):X} get_register(0x{int(register):04X}) = 0x{int(result):04X}"
        )
        return result

    def apply_registers(
        self,
        reg_list,
        reg_size: DataWidth,
        val_size: DataWidth,
        endian: Endianness,
        delay_sec=0.01,
    ):
        for i2c_address, register, value in reg_list:
            if register == d457_registers.REALSENSE_TABLE_WAIT_MS:
                time.sleep(0.5)
            else:
                self.set_register(
                    i2c_address, register, value, reg_size, val_size, endian
                )
            time.sleep(delay_sec)

    def configure_converter(self, converter):
        (
            frame_start_size,
            frame_end_size,
            line_start_size,
            line_end_size,
        ) = self._hololink.csi_size()
    
        metadata_size = line_start_size + 68 + line_end_size
        logging.debug(
            f"[Realsense] Configuring converter with frame_start_size={frame_start_size}, \
              frame_end_size={frame_end_size}, line_start_size={line_start_size}, \
                line_end_size={line_end_size}, metadata_size={metadata_size}"
        )
        converter.configure(
            self._width,
            self._height,
            self._pixel_format,
            frame_start_size + metadata_size,
            frame_end_size,
            line_start_size,
            line_end_size,
        )

    def configure_camera(self, realsense_mode):
        logging.info("[Realsense] Applying MAX92595 MAX9296 REG WRITES")

        self.apply_registers(
            d457_registers.MAX9295_MAX9296_REG_INIT_WRITES,
            reg_size=DataWidth.BITS_16,
            val_size=DataWidth.BITS_8,
            endian=Endianness.BIG,
        )

        self.set_mode(realsense_mode)

    def set_digital_gain_reg(self, val):
        logging.info(f"[Realsense] Digital gain set to: {val}")

    def pixel_format(self):
        return self._pixel_format

    def test_pattern(self, enable=False):
        logging.info("Test pattern control is not implemented for RealSense.")
