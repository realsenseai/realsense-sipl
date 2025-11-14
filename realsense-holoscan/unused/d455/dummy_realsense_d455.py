"""
SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
SPDX-License-Identifier: Apache-2.0
"""

import logging
import time

import hololink as hololink_module

from li_i2c_expander import LII2CExpander, I2C_Expander_Output_EN

# Camera info
DRIVER_NAME = "REALSENSE-D455"
VERSION = 1

# RealSense I2C camera address (placeholder — confirm your setup)
CAM_I2C_ADDRESS = 0x1A


class DummyRealsenseCamD455:
    def __init__(
        self,
        hololink_channel=None,
        i2c_controller_address=hololink_module.CAM_I2C_CTRL,
        expander_configuration=0,
    ):

        self._width = 1280
        self._height = 720
        self._mode = 0

    def setup_clock(self):
        pass

    def set_mode(self, mode):
        logging.info(f"[Realsense Camera] Mode set to: {mode}")
        self._mode = mode

    def power_on(self):
        """Enable power to RealSense via I2C expander"""
        # self._i2c_expander.configure(self._i2c_expander_configuration.value)
        time.sleep(0.1)

    def configure(self, camera_mode):
        """Configure the camera (if needed) over I2C"""
        self.power_on()
        logging.info("Sending RealSense config over I2C...")


    def start(self):
        logging.info("RealSenseCam: start()")

    def stop(self):
        logging.info("RealSenseCam: stop()")

    def _write_register(self, i2c_addr, reg_high, reg_low, data):
        reg = (reg_high << 8) | reg_low
        write_bytes = bytearray(100)
        serializer = hololink_module.Serializer(write_bytes)
        serializer.append_uint16_be(reg)
        serializer.append_uint8(data)
        self._i2c.i2c_transaction(
            i2c_addr,
            write_bytes[:serializer.length()],
            read_bytes=0
        )

    def _apply_registers(self, reg_list):
        for i2c_addr, reg_high, reg_low, data in reg_list:
            self._write_register(i2c_addr, reg_high, reg_low, data)
            time.sleep(0.005)

    def get_register(self, register):
        self._i2c_expander.configure(self._i2c_expander_configuration.value)
        write_bytes = bytearray(100)
        serializer = hololink_module.Serializer(write_bytes)
        serializer.append_uint16_be(register)
        reply = self._i2c.i2c_transaction(
            CAM_I2C_ADDRESS, write_bytes[:serializer.length()], read_bytes=1
        )
        deserializer = hololink_module.Deserializer(reply)
        return deserializer.next_uint8()

    def set_register(self, register, value):
        self._i2c_expander.configure(self._i2c_expander_configuration.value)
        write_bytes = bytearray(100)
        serializer = hololink_module.Serializer(write_bytes)
        serializer.append_uint16_be(register)
        serializer.append_uint8(value)
        self._i2c.i2c_transaction(
            CAM_I2C_ADDRESS,
            write_bytes[:serializer.length()],
            read_bytes=0,
        )

    def configure_converter(self, converter):
        (
            frame_start_size,
            frame_end_size,
            line_start_size,
            line_end_size,
        ) = self._hololink.csi_size()

        converter.configure(
            self._width,
            self._height,
            self._pixel_format,
            frame_start_size,
            frame_end_size,
            line_start_size,
            line_end_size,
            margin_top=0,
        )

    def set_digital_gain_reg(self, val):
        logging.info(f"[Realsense] Digital gain set to: {val}")

    def pixel_format(self):
        return self._pixel_format

    def test_pattern(self, enable=False):
        logging.info("Test pattern control is not implemented for RealSense.")


class DummyRealsenseCameraMode:
    def __init__(self, value):
        self.value = value

    def __str__(self):
        return f"RealsenseMode({self.value})"
