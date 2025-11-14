#!/usr/bin/env python3
#
# SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: LicenseRef-NvidiaProprietary
#
# NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
# property and proprietary rights in and to this material, related
# documentation and any modifications thereto. Any use, reproduction,
# disclosure or distribution of this material and related documentation
# without an express license agreement from NVIDIA CORPORATION or
# its affiliates is strictly prohibited.

from pathlib import Path
import socket


class opal:

    HEADER_SIZE = 6

    MAGIC = 42

    # daemon response flavors
    FLAVOR_BOOL       = 0
    FLAVOR_UINT8      = 1
    FLAVOR_UINT16     = 2
    FLAVOR_UINT32     = 4
    FLAVOR_CHAR       = 5

    # client query flavors
    FLAVOR_BLOB       = 33
    FLAVOR_EXEC_QUERY = 34
    FLAVOR_READ_GPIO  = 36
    FLAVOR_WRITE_GPIO = 37
    FLAVOR_READ_I2C   = 38
    FLAVOR_WRITE_I2C  = 39

    FLAVOR_CLEAR_DEVICE_TABLE = 100
    FLAVOR_ADD_DEVICE         = 101

    FLAVOR_SEQUENCE_START     = 112
    FLAVOR_SEQUENCE_BYTECODE  = 113
    FLAVOR_SEQUENCE_END       = 114

    @staticmethod
    def header(flavor, payload_size):
        total_size = opal.HEADER_SIZE + payload_size
        h = bytearray(total_size)
        h[0] = opal.MAGIC
        h[1] = flavor
        opal.pack_u32(h, 2, total_size)
        return h

    @staticmethod
    def set_payload_size(header, payload_size):
        total_size = opal.HEADER_SIZE + payload_size
        opal.pack_u32(header, 2, total_size)

    @staticmethod
    def pack_bool(data, offset, value):
        opal.pack_u8(data, offset, 1 if value else 0)
        return offset + 1

    @staticmethod
    def unpack_bool(data, offset):
        return (opal.unpack_u8(data, offset) != 0)

    @staticmethod
    def pack_u8(data, offset, value):
        data[offset] = value & 0xFF
        return offset + 1

    @staticmethod
    def pack_u16(data, offset, value):
        data[offset  ] = (value >> 8) & 0xFF
        data[offset+1] = (value     ) & 0xFF
        return offset + 2

    @staticmethod
    def pack_u32(data, offset, value):
        data[offset  ] = (value >> 24) & 0xFF
        data[offset+1] = (value >> 16) & 0xFF
        data[offset+2] = (value >>  8) & 0xFF
        data[offset+3] = (value      ) & 0xFF
        return offset + 4

    @staticmethod
    def unpack_u8(data, offset):
        return data[offset]

    @staticmethod
    def unpack_u16(data, offset):
        value  = data[offset  ] << 8
        value |= data[offset+1]
        return value

    @staticmethod
    def unpack_u32(data, offset):
        value  = data[offset  ] << 24
        value |= data[offset+1] << 16
        value |= data[offset+2] <<  8
        value |= data[offset+3]
        return value

    @staticmethod
    def unpack(flavor, payload):
        value = None
        if len(payload) > 0:
            match flavor:

                case opal.FLAVOR_BOOL:
                    if len(payload) == 1:
                        value = opal.unpack_bool(payload, 0)
                    else:
                        value = []
                        i = 0
                        while len(payload) >= (i+1):
                            value.append(opal.unpack_bool(payload, i))
                            i += 1

                case opal.FLAVOR_UINT8:
                    if len(payload) == 1:
                        value = opal.unpack_u8(payload, 0)
                    else:
                        value = []
                        i = 0
                        while len(payload) >= (i+1):
                            value.append(opal.unpack_u8(payload, i))
                            i += 1

                case opal.FLAVOR_UINT16:
                    if len(payload) == 2:
                        value = opal.unpack_u16(payload, 0)
                    else:
                        value = []
                        i = 0
                        while len(payload) >= (i+2):
                            value.append(opal.unpack_u16(payload, i))
                            i += 2

                case opal.FLAVOR_UINT32:
                    if len(payload) == 4:
                        value = opal.unpack_u32(payload, 0)
                    else:
                        value = []
                        i = 0
                        while len(payload) >= (i+4):
                            value.append(opal.unpack_u32(payload, i))
                            i += 4

                case opal.FLAVOR_CHAR:
                    if len(payload) > 0:
                        value = payload[:-1].decode('utf-8')
                    else:
                        value = None

                case _:
                    value = None
        return value


class opalClient:

    __slots__ = ('host', 'port', 'sock')

    def __init__(self, host:str, port:int):
        self.host = host
        self.port = port
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((host, port))

    def __del__(self):
        self.sock.close()

    def __str__(self):
        return f'opalClient({repr(host)},{port})'

    def __repr__(self):
        return self.__str__()

    def get_payload(self):
        header = self.sock.recv(opal.HEADER_SIZE)
        if len(header) != opal.HEADER_SIZE:
            raise Exception("connection lost")
        magic  = opal.unpack_u8(header, 0)
        flavor = opal.unpack_u8(header, 1)
        payload_size = opal.unpack_u32(header, 2)
        payload_size -= opal.HEADER_SIZE
        if (magic != opal.MAGIC):
            raise Exception("bad magic")
        if (payload_size < 0) or (payload_size > 0xffffff):
            raise Exception("bad payload size")
        payload = self.sock.recv(payload_size)
        if len(payload) != payload_size:
            raise Exception("connection lost")
        return (flavor, payload)

    def response(self):
        flavor,payload = self.get_payload()
        return opal.unpack(flavor, payload)

    def exec_blob_file(self, blob_name:str):
        status = False
        with open(blob_name,'rb') as f:
            blob = f.read()
            status = self.exec_blob(blob)
        return status

    def exec_blob(self, blob):
        msg = opal.header(opal.FLAVOR_BLOB, 0)
        opal.set_payload_size(msg, len(blob))
        self.sock.sendall(msg)
        self.sock.sendall(blob)
        return self.response()

    def exec_query(self):
        msg = opal.header(opal.FLAVOR_EXEC_QUERY, 0)
        self.sock.sendall(msg)
        return self.response()

    def read_gpio(self, pin_number:int):
        msg = opal.header(opal.FLAVOR_READ_GPIO, 2)
        i = opal.HEADER_SIZE
        i = opal.pack_u16(msg, i, pin_number)
        self.sock.sendall(msg)
        return self.response()

    def write_gpio(self, pin_number:int, value:int):
        msg = opal.header(opal.FLAVOR_WRITE_GPIO, 3)
        i = opal.HEADER_SIZE
        i = opal.pack_u16(msg, i, pin_number)
        i = opal.pack_bool(msg, i, (value != 0))
        self.sock.sendall(msg)
        return self.response()

    def read_i2c(self, index, offset):
        msg = opal.header(opal.FLAVOR_READ_I2C, 4)
        i = opal.HEADER_SIZE
        i = opal.pack_u16(msg, i, index)
        i = opal.pack_u16(msg, i, offset)
        self.sock.sendall(msg)
        return self.response()

    def write_i2c(self, index:int, offset:int, value:int):
        msg = opal.header(opal.FLAVOR_WRITE_I2C, 6)
        i = opal.HEADER_SIZE
        i = opal.pack_u16(msg, i, index)
        i = opal.pack_u16(msg, i, offset)
        i = opal.pack_u16(msg, i, value)
        self.sock.sendall(msg)
        return self.response()

    def clear_device_table(self):
        msg = opal.header(opal.FLAVOR_CLEAR_DEVICE_TABLE, 0)
        self.sock.sendall(msg)
        return self.response()

    def add_device(self, address:int, offset_bits:int, data_bits:int, flags:int=0):
        msg = opal.header(opal.FLAVOR_ADD_DEVICE, 4)
        i = opal.HEADER_SIZE
        i = opal.pack_u8(msg, i, address)
        i = opal.pack_u8(msg, i, flags)
        i = opal.pack_u8(msg, i, offset_bits>>3)
        i = opal.pack_u8(msg, i, data_bits>>3)
        self.sock.sendall(msg)
        return self.response()

    def sequence_start(self):
        msg = opal.header(opal.FLAVOR_SEQUENCE_START, 0)
        self.sock.sendall(msg)
        return self.response()

    def sequence_bytecode(self, bytecode):
        msg = opal.header(opal.FLAVOR_SEQUENCE_BYTECODE, 0)
        opal.set_payload_size(msg, len(bytecode))
        self.sock.sendall(msg)
        self.sock.sendall(bytecode)
        return self.response()

    def sequence_end(self):
        msg = opal.header(opal.FLAVOR_SEQUENCE_END, 0)
        self.sock.sendall(msg)
        return self.response()
