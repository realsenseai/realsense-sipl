# SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: LicenseRef-NvidiaProprietary
#
# NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
# property and proprietary rights in and to this material, related
# documentation and any modifications thereto. Any use, reproduction,
# disclosure or distribution of this material and related documentation
# without an express license agreement from NVIDIA CORPORATION or
# its affiliates is strictly prohibited.

"""
CRC-16/MCRF4XX Utility Functions

Polynomial = 0x1021
Initial Value = 0xFFFF
"""

from collections.abc import Sequence
from enum import Enum
import struct
import warnings

crc16_table = [
    0x0000, 0x1189, 0x2312, 0x329B, 0x4624, 0x57AD, 0x6536, 0x74BF,
    0x8C48, 0x9DC1, 0xAF5A, 0xBED3, 0xCA6C, 0xDBE5, 0xE97E, 0xF8F7,
    0x1081, 0x0108, 0x3393, 0x221A, 0x56A5, 0x472C, 0x75B7, 0x643E,
    0x9CC9, 0x8D40, 0xBFDB, 0xAE52, 0xDAED, 0xCB64, 0xF9FF, 0xE876,
    0x2102, 0x308B, 0x0210, 0x1399, 0x6726, 0x76AF, 0x4434, 0x55BD,
    0xAD4A, 0xBCC3, 0x8E58, 0x9FD1, 0xEB6E, 0xFAE7, 0xC87C, 0xD9F5,
    0x3183, 0x200A, 0x1291, 0x0318, 0x77A7, 0x662E, 0x54B5, 0x453C,
    0xBDCB, 0xAC42, 0x9ED9, 0x8F50, 0xFBEF, 0xEA66, 0xD8FD, 0xC974,
    0x4204, 0x538D, 0x6116, 0x709F, 0x0420, 0x15A9, 0x2732, 0x36BB,
    0xCE4C, 0xDFC5, 0xED5E, 0xFCD7, 0x8868, 0x99E1, 0xAB7A, 0xBAF3,
    0x5285, 0x430C, 0x7197, 0x601E, 0x14A1, 0x0528, 0x37B3, 0x263A,
    0xDECD, 0xCF44, 0xFDDF, 0xEC56, 0x98E9, 0x8960, 0xBBFB, 0xAA72,
    0x6306, 0x728F, 0x4014, 0x519D, 0x2522, 0x34AB, 0x0630, 0x17B9,
    0xEF4E, 0xFEC7, 0xCC5C, 0xDDD5, 0xA96A, 0xB8E3, 0x8A78, 0x9BF1,
    0x7387, 0x620E, 0x5095, 0x411C, 0x35A3, 0x242A, 0x16B1, 0x0738,
    0xFFCF, 0xEE46, 0xDCDD, 0xCD54, 0xB9EB, 0xA862, 0x9AF9, 0x8B70,
    0x8408, 0x9581, 0xA71A, 0xB693, 0xC22C, 0xD3A5, 0xE13E, 0xF0B7,
    0x0840, 0x19C9, 0x2B52, 0x3ADB, 0x4E64, 0x5FED, 0x6D76, 0x7CFF,
    0x9489, 0x8500, 0xB79B, 0xA612, 0xD2AD, 0xC324, 0xF1BF, 0xE036,
    0x18C1, 0x0948, 0x3BD3, 0x2A5A, 0x5EE5, 0x4F6C, 0x7DF7, 0x6C7E,
    0xA50A, 0xB483, 0x8618, 0x9791, 0xE32E, 0xF2A7, 0xC03C, 0xD1B5,
    0x2942, 0x38CB, 0x0A50, 0x1BD9, 0x6F66, 0x7EEF, 0x4C74, 0x5DFD,
    0xB58B, 0xA402, 0x9699, 0x8710, 0xF3AF, 0xE226, 0xD0BD, 0xC134,
    0x39C3, 0x284A, 0x1AD1, 0x0B58, 0x7FE7, 0x6E6E, 0x5CF5, 0x4D7C,
    0xC60C, 0xD785, 0xE51E, 0xF497, 0x8028, 0x91A1, 0xA33A, 0xB2B3,
    0x4A44, 0x5BCD, 0x6956, 0x78DF, 0x0C60, 0x1DE9, 0x2F72, 0x3EFB,
    0xD68D, 0xC704, 0xF59F, 0xE416, 0x90A9, 0x8120, 0xB3BB, 0xA232,
    0x5AC5, 0x4B4C, 0x79D7, 0x685E, 0x1CE1, 0x0D68, 0x3FF3, 0x2E7A,
    0xE70E, 0xF687, 0xC41C, 0xD595, 0xA12A, 0xB0A3, 0x8238, 0x93B1,
    0x6B46, 0x7ACF, 0x4854, 0x59DD, 0x2D62, 0x3CEB, 0x0E70, 0x1FF9,
    0xF78F, 0xE606, 0xD49D, 0xC514, 0xB1AB, 0xA022, 0x92B9, 0x8330,
    0x7BC7, 0x6A4E, 0x58D5, 0x495C, 0x3DE3, 0x2C6A, 0x1EF1, 0x0F78
]

class CRC_16_MCRF4XX:
    """
    A CRC-16/MCRF4XX calculator.
    """

    def __init__(self, initial_value:int=0xFFFF, final_xor:int=0x0000):
        self.initial_value = initial_value
        self.final_xor = final_xor
        self.crc = initial_value
        self.bytes = 0

    def update(self, data:bytes):
        for byte in data:
            self.crc = crc16_table[(self.crc ^ byte) & 0xFF] ^ ((self.crc >> 8) & 0xFF)
            self.bytes += 1
        return self.crc ^ self.final_xor

    def reset(self):
        self.crc = self.initial_value
        self.bytes = 0

    def __str__(self):
        return f'CRC-16 (0x{self.crc:04X}, {self.bytes} bytes)'

    def __repr__(self):
        return self.__str__()

    def value(self):
        return self.crc ^ self.final_xor

    def count(self):
        return self.bytes


class CommunicationCRC:
    """
    A class to calculate the CRC and byte counts of a communication stream.
    A class to calculate the CRC and byte counts of a communication stream.
    """

    def __init__(self, offset_bytes:int, data_bytes:int):
        self.rd_crc = CRC_16_MCRF4XX()
        self.wr_crc = CRC_16_MCRF4XX()
        self.data_bytes = data_bytes
        self.offset_bytes = offset_bytes

    def _pack_data(self, data:int):
        return struct.pack('>H' if self.data_bytes == 2 else 'B', data)

    def _pack_offset(self, offset:int):
        return struct.pack('>H' if self.offset_bytes == 2 else 'B', offset)

    def _pack_address(self, address:int, write:bool):
        return struct.pack('B', address << 1 | (0x0 if write else 0x1))

    def _pack_data_bytes(self, data:Sequence):
        data_bytes = bytearray()
        for byte in data:
            data_bytes.extend(self._pack_data(byte))
        return data_bytes

    def _pack_data(self, data:int):
        return struct.pack('>H' if self.data_bytes == 2 else 'B', data)

    def write(self, address:int, offset:int, data:int):
        addr = self._pack_address(address, write=True)
        register = self._pack_offset(offset)
        value = self._pack_data(data)

        self.wr_crc.update(addr + register + value)

    def write_stream(self, address:int, offset:int, data:Sequence):
        addr = self._pack_address(address, write=True)
        register = self._pack_offset(offset)
        values = self._pack_data_bytes(data)

        self.wr_crc.update(addr + register + values)

    def write_masked(self, address:int, offset:int, existing:int, modified:int, mask:int):
        self.read(address, offset, existing)
        self.write(address, offset, (existing & ~mask) | (modified & mask))

    def read(self, address:int, offset:int, data:int):
        write_addr = self._pack_address(address, write=True)
        read_addr = self._pack_address(address, write=False)
        register = self._pack_offset(offset)
        value = self._pack_data(data)

        self.wr_crc.update(write_addr + register + read_addr)
        self.rd_crc.update(value)
        warnings.warn("Read CRC calculation for a blind I2C read may be unreliable.")

    def read_stream(self, address:int, offset:int, data:Sequence):
        write_addr = self._pack_address(address, write=True)
        read_addr = self._pack_address(address, write=False)
        register = self._pack_offset(offset)
        values = self._pack_data_bytes(data)

        self.wr_crc.update(write_addr + register + read_addr)
        self.rd_crc.update(values)
        warnings.warn("Read CRC calculation for a blind I2C read may be unreliable.")

    def read_verify(self, address:int, offset:int, expected:int, mask:int):
        self.read(address, offset, expected & mask)

    def poll(self, address:int, offset:int, expected:int, mask:int, interval:int, retries:int):
        # We make a naive assumption that the poll completes in a single interval
        self.read(address, offset, expected)

    def reset(self):
        self.rd_crc.reset()
        self.wr_crc.reset()

    def read_count(self):
        return self.rd_crc.count()

    def write_count(self):
        return self.wr_crc.count()

    def read_crc(self):
        return self.rd_crc.value()

    def write_crc(self):
        return self.wr_crc.value()

    def value(self):
        return self.rd_crc.value(), self.wr_crc.value()

    def __str__(self):
        return f'Read {self.rd_crc}, Write {self.wr_crc}'

    def __repr__(self):
        return self.__str__()
