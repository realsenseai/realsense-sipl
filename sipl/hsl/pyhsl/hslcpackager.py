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
#
# hslcpackager: package and unpackage HSLC files.

import hashlib
import io
import logging
import uuid
from typing import BinaryIO
from pyhslcore import *

UUID_BBF_FILE_FORMAT_V1 = uuid.UUID('49fcb51e-2cac-4aa9-855d-0f692ffe82ed') # BBF file format version 1 (copied from Tuna)
UUID_BBF_TYPE_HASH      = uuid.UUID('56059629-1239-4afc-a322-9a56dca02c85') # BBF hash chunk UUID (copied from Tuna)
UUID_HSL_SEQUENCE       = uuid.UUID('9f244a51-a29b-44b3-bbef-df79ea427068') # HSL sequence chunk UUID

CHUNK_TYPE_HEADER   = bytes('HDR1', 'utf-8') # Header chunk type
CHUNK_TYPE_BYTECODE = bytes('BYT1', 'utf-8') # Bytecode chunk type

CHUNK_HEADER_NAME_LENGTH = 64

class HslcBlob:

    """
    A blob of HSL bytecode with associated metadata.

    Attributes:
        name: Name identifier for this blob (max 16 chars)
        major_version: Major version number of the bytecode format
        minor_version: Minor version number of the bytecode format
        bytecode: The actual HSL bytecode bytes
    """

    __slots__ = ('name', 'major_version', 'minor_version', 'bytecode')

    def __init__(self, name: str, major_version: int, minor_version: int, bytecode: bytes):
        if (len(name) > CHUNK_HEADER_NAME_LENGTH):
            raise PyHslException(f'Blob name "{name}" is too long')
        self.name = name
        self.major_version = major_version
        self.minor_version = minor_version
        self.bytecode = bytecode

    # Package the blob into a bytearray.
    def package(self) -> bytearray:
        header_chunk = bytearray()
        header_chunk_name = bytearray(CHUNK_HEADER_NAME_LENGTH)
        header_chunk_name[0:len(self.name)] = bytes(self.name, 'utf-8')
        header_chunk.extend(header_chunk_name)
        header_chunk.extend(self.major_version.to_bytes(1, 'little'))
        header_chunk.extend(self.minor_version.to_bytes(1, 'little'))

        internals = bytearray()
        internals.extend(CHUNK_TYPE_HEADER)
        internals.extend(len(header_chunk).to_bytes(4, 'little'))
        internals.extend(header_chunk)

        internals.extend(CHUNK_TYPE_BYTECODE)
        internals.extend(len(self.bytecode).to_bytes(4, 'little'))
        internals.extend(self.bytecode)

        contents = bytearray()
        contents.extend(UUID_HSL_SEQUENCE.bytes)
        contents.extend(len(internals).to_bytes(8, 'little'))
        contents.extend(internals)
        return contents

    # Unpackage a bytearray into an HslcBlob.
    @staticmethod
    def unpackage(contents: bytes) -> 'HslcBlob':
        input_stream = io.BytesIO(contents)
        header_chunk_type = input_stream.read(4)
        pyhslassert(header_chunk_type == CHUNK_TYPE_HEADER, 'Invalid header chunk type (expecting HDR1)')
        header_chunk_length = int.from_bytes(input_stream.read(4), 'little')
        pyhslassert(header_chunk_length == CHUNK_HEADER_NAME_LENGTH + 2, f'Invalid header chunk length: {header_chunk_length}')
        header_chunk = input_stream.read(header_chunk_length)
        blob_name = header_chunk[0:CHUNK_HEADER_NAME_LENGTH].decode('utf-8').rstrip('\x00')
        major_version = int.from_bytes(header_chunk[CHUNK_HEADER_NAME_LENGTH:CHUNK_HEADER_NAME_LENGTH+1], 'little')
        minor_version = int.from_bytes(header_chunk[CHUNK_HEADER_NAME_LENGTH+1:CHUNK_HEADER_NAME_LENGTH+2], 'little')

        bytecode_chunk_type = input_stream.read(4)
        pyhslassert(bytecode_chunk_type == CHUNK_TYPE_BYTECODE, 'Invalid sequence chunk type (expecting BYT1)')
        bytecode_chunk_length = int.from_bytes(input_stream.read(4), 'little')
        bytecode = input_stream.read(bytecode_chunk_length)

        return HslcBlob(blob_name, major_version, minor_version, bytecode)

class HslcPackager:
    """
    A class for packaging and unpackaging HslcBlob objects into and from HSLC files.

    Attributes:
        blobs: A dictionary of HslcBlob objects, keyed by their names
        logger: A logger for logging messages
    """
    __slots__ = ('blobs', 'logger')

    def __init__(self, logger: logging.Logger = None):
        self.blobs = {}
        self.logger = logger
        if not self.logger:
            logging.basicConfig()
            self.logger = logging.getLogger('hslcpackager')

    def add_blob(self, blob: HslcBlob):
        pyhslassert(blob.name not in self.blobs, f'Blob {blob.name} already exists')
        self.blobs[blob.name] = blob

    def get_blob(self, name: str) -> HslcBlob:
        try:
            return self.blobs[name]
        except KeyError:
            raise PyHslException(f'Blob {name} not found')

    def get_blob_names(self) -> list[str]:
        return [name for name in self.blobs.keys()]

    # Package the HslcBlob objects into an HSLC file.
    def package(self, output_file: BinaryIO):
        contents = bytearray()

        # Add header chunk (with a zero-length tool info string)
        contents.extend(UUID_BBF_FILE_FORMAT_V1.bytes)
        contents.extend(int(0).to_bytes(8, 'little'))

        # Add blob chunks
        for blob in self.blobs.values():
            contents.extend(blob.package())

        # Add hash chunk
        file_hash = hashlib.sha256(contents).digest()
        contents.extend(UUID_BBF_TYPE_HASH.bytes)
        contents.extend(len(file_hash).to_bytes(8, 'little'))
        contents.extend(file_hash)

        output_file.write(contents)

    # Unpackage an HSLC file into HslcBlob objects.
    def unpackage(self, input_file: BinaryIO):
        pyhslassert(len(self.blobs) == 0, 'Cannot unpackage into an HslcPackager that already contains blobs')

        # Get the file size
        file_size = input_file.seek(0, 2)
        input_file.seek(0, 0)

        # Read header chunk
        header_chunk_uuid = uuid.UUID(bytes=input_file.read(16))
        pyhslassert(header_chunk_uuid == UUID_BBF_FILE_FORMAT_V1, 'Invalid header chunk UUID')
        header_chunk_toolstring_length = int.from_bytes(input_file.read(8), 'little')
        header_chunk_toolstring = input_file.read(header_chunk_toolstring_length).decode('utf-8')
        self.logger.debug(f'Header chunk toolstring: "{header_chunk_toolstring}"')

        # Read remaining chunks (hash chunk should be last)
        while True:
            chunk_start_position = input_file.tell()
            pyhslassert(chunk_start_position < file_size, 'Unexpected end of file')
            chunk_uuid = uuid.UUID(bytes=input_file.read(16))
            chunk_length = int.from_bytes(input_file.read(8), 'little')
            if chunk_uuid == UUID_HSL_SEQUENCE:
                chunk_contents = input_file.read(chunk_length)
                self.add_blob(HslcBlob.unpackage(chunk_contents))
            elif chunk_uuid == UUID_BBF_TYPE_HASH:
                pyhslassert(chunk_length == 32, 'Invalid hash chunk length')
                file_hash = input_file.read(chunk_length)
                input_file.seek(0, 0)
                contents = input_file.read(chunk_start_position)
                pyhslassert(file_hash == hashlib.sha256(contents).digest(), 'Invalid file hash')
                break
            else:
                self.logger.warning(f'Skipping chunk with unknown UUID: {chunk_uuid}')
                input_file.seek(chunk_length, 1)
