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
# pyhsl: support classes and functions for creating HSL sequences.

from pyhslcore import *
from nvhslencoderpy import *
from hslcpackager import *
import collections
import logging
import tempfile
from crcutils import CommunicationCRC

class HSLRecorder:
    """
    The HSLRecorder class, which creates a bytecode blob using the C++ HSL Encoder.

    Access to the HSL Encoder is exposed by pybind11, which created the nvhslencoderpy
    library.
    """

    encoder_result_strings = {
        EncoderResult.OK: "OK",
        EncoderResult.BAD_PARAMS: "BAD_PARAMS",
        EncoderResult.OUT_OF_SPACE: "OUT_OF_SPACE",
        EncoderResult.NOT_IMPLEMENTED: "NOT_IMPLEMENTED",
        EncoderResult.BAD_STATE: "BAD_STATE"
    }

    s_output_path = Path(".")

    __slots__ = ('encoder',)

    def __init__(self, max_blob_size:int):
        self.encoder = HslEncoder(max_blob_size)

    def __str__(self):
        return 'HSLRecorder()'

    def __repr__(self):
        return self.__str__()

    def _erstring(self, result:EncoderResult):
        return HSLRecorder.encoder_result_strings.get(result, f"UNKNOWN_ERROR: {result}")

    def addI2CDevice(self, device):
        result = self.encoder.addI2CDevice(device.address, device.device_flags, device.offset_bytes, device.data_bytes)
        pyhslassert(result == EncoderResult.OK, f'Failed to add I2C device: {self._erstring(result)}')

    def addGPIOPin(self, device):
        result = self.encoder.addGPIOPin(device.physicalName, device.flags)
        pyhslassert(result == EncoderResult.OK, f'Failed to add GPIO device: {self._erstring(result)}')

    def addFence(self, fence):
        result = self.encoder.addFence(fence.address, fence.threshold)
        pyhslassert(result == EncoderResult.OK, f'Failed to add fence: {self._erstring(result)}')

    def addMemory(self, block):
        result = self.encoder.addMemory(block.address, block.size)
        pyhslassert(result == EncoderResult.OK, f'Failed to add memory block: {self._erstring(result)}')

    def writeI2C(self, address:int, offset:int, data:int, flags:int):
        result = self.encoder.writeI2C(address, offset, data, flags)
        pyhslassert(result == EncoderResult.OK, f'Failed to write I2C: {self._erstring(result)}')

    def writeStreamI2C(self, address:int, startOffset:int, data:list, flags:int):
        result = self.encoder.writeStreamI2C(address, startOffset, data, flags)
        pyhslassert(result == EncoderResult.OK, f'Failed to write I2C stream: {self._erstring(result)}')

    def writeMaskedI2C(self, address:int, offset:int, data:int, mask:int):
        result = self.encoder.writeMaskedI2C(address, offset, data, mask)
        pyhslassert(result == EncoderResult.OK, f'Failed to masked I2C write: {self._erstring(result)}')

    def pollI2C(self, address:int, offset:int, expectedValue:int, mask:int, intervalInUsec:int, retries:int):
        result = self.encoder.pollI2C(address, offset, expectedValue, mask, intervalInUsec, retries)
        pyhslassert(result == EncoderResult.OK, f'Failed to poll I2C: {self._erstring(result)}')

    def readVerifyI2C(self, address:int, offset:int, expectedValue:int, mask:int):
        result = self.encoder.readVerifyI2C(address, offset, expectedValue, mask)
        pyhslassert(result == EncoderResult.OK, f'Failed to verify I2C read: {self._erstring(result)}')

    def readVerifyStreamI2C(self, address:int, startOffset:int, expectedValues:list):
        result = self.encoder.readVerifyStreamI2C(address, startOffset, expectedValues)
        pyhslassert(result == EncoderResult.OK, f'Failed to verify I2C read stream: {self._erstring(result)}')

    def readDiscardI2C(self, address:int, offset:int):
        result = self.encoder.readDiscardI2C(address, offset)
        pyhslassert(result == EncoderResult.OK, f'Failed to read discard I2C: {self._erstring(result)}')

    def delay(self, delay_in_usec:int):
        result = self.encoder.delay(delay_in_usec)
        pyhslassert(result == EncoderResult.OK, f'Failed to delay: {self._erstring(result)}')

    def annotate(self, comment:str):
        result = self.encoder.annotate(comment)
        pyhslassert(result == EncoderResult.OK, f'Failed to annotate: {self._erstring(result)}')

    def writeGPIO(self, physicalAddress:int, value:bool):
        result = self.encoder.writeGPIO(physicalAddress, value)
        pyhslassert(result == EncoderResult.OK, f'Failed to write GPIO: {self._erstring(result)}')

    def readVerifyGPIO(self, physicalAddress:int, value:bool):
        result = self.encoder.readVerifyGPIO(physicalAddress, value)
        pyhslassert(result == EncoderResult.OK, f'Failed to readVerify GPIO: {self._erstring(result)}')

    def pollGPIO(self, physicalAddress:int, value:bool, intervalInUsec:int, retries:int):
        result = self.encoder.pollGPIO(physicalAddress, value, intervalInUsec, retries)
        pyhslassert(result == EncoderResult.OK, f'Failed to poll GPIO: {self._erstring(result)}')

    def waitFence(self, address:int, timeoutInUsec:int):
        result = self.encoder.waitFence(address, timeoutInUsec)
        pyhslassert(result == EncoderResult.OK, f'Failed to wait for fence: {self._erstring(result)}')

    def signalFence(self, address:int):
        result = self.encoder.signalFence(address)
        pyhslassert(result == EncoderResult.OK, f'Failed to signal fence: {self._erstring(result)}')

    def waitInternalSemaphore(self, semaphoreIndex:int, timeoutInUsec:int):
        result = self.encoder.waitInternalSemaphore(semaphoreIndex, timeoutInUsec)
        pyhslassert(result == EncoderResult.OK, f'Failed to wait for internal semaphore: {self._erstring(result)}')

    def signalInternalSemaphore(self, semaphoreIndex:int):
        result = self.encoder.signalInternalSemaphore(semaphoreIndex)
        pyhslassert(result == EncoderResult.OK, f'Failed to signal internal semaphore: {self._erstring(result)}')

    def writeI2CFromMemory(self, device:int, memAddress:int, i2cOffset:int, memOffset:int, byteCount:int):
        result = self.encoder.writeI2CFromMemory(device, memAddress, i2cOffset, memOffset, byteCount)
        pyhslassert(result == EncoderResult.OK, f'Failed to write I2C from memory: {self._erstring(result)}')

    def readI2CToMemory(self, device:int, memAddress:int, i2cOffset:int, memOffset:int, byteCount:int):
        result = self.encoder.readI2CToMemory(device, memAddress, i2cOffset, memOffset, byteCount)
        pyhslassert(result == EncoderResult.OK, f'Failed to read I2C to memory: {self._erstring(result)}')

    def writeTimestampToMemory(self, memAddress:int, memOffset:int):
        result = self.encoder.writeTimestampToMemory(memAddress, memOffset)
        pyhslassert(result == EncoderResult.OK, f'Failed to write timestamp to memory: {self._erstring(result)}')

    def writeToFile(self, filename:str):
        filepath = str(Configuration.s_output_dir / filename)
        result = writeBlobToFile(self.encoder, filepath)
        pyhslassert(result, f'Failed to write to HSL file {filepath}')

    def recordBytecode(self, packager:HslcPackager, name:str):
        major_version = self.encoder.getMajorVersion()
        minor_version = self.encoder.getMinorVersion()
        with tempfile.NamedTemporaryFile(delete=True) as fw:
            writeBlobToFile(self.encoder, fw.name)
            with open(fw.name, 'rb') as fr:
                bytecode = fr.read()
            packager.add_blob(HslcBlob(name, major_version, minor_version, bytecode))

    @staticmethod
    def setOutputPath(path:Path):
        HSLRecorder.s_output_path = path

TIMESTAMP_SIZE = 8

class I2CDevice:

    """
    A single device on an I2C bus.

    Each device has an I2C address and information on the size of both offsets and data.
    I2C operations are added to the HSL blob via methods on this class.
    """

    s_current_device = None     # I2C device currently in use (inside a "with" block)
    s_all_devices = []

    __slots__ = (
        'logger', 'address', 'offset_bytes', 'data_bytes', 'name', 'write_flags', 'device_flags',
        'communication_crc', 'aliases', 'parent_device'
    )

    def __init__(self, address:int, offset_width:int, data_width:int, name:str, auto_retry:bool=False,
                 ten_bit_address:bool=False, use_crc:bool=False):
        self.logger = ph_logger()
        self.address = address
        pyhslassert(offset_width in (8,16), "Bad offset width")
        pyhslassert(data_width in (8,16), "Bad data width")
        self.offset_bytes = int(offset_width / 8)
        self.data_bytes = int(data_width / 8)
        self.name = name
        self.write_flags = 0
        self.device_flags =  I2C_DEVICE_AUTO_RETRY     if auto_retry      else 0
        self.device_flags |= I2C_DEVICE_10_BIT_ADDRESS if ten_bit_address else 0
        I2CDevice.s_all_devices.append(self)
        self.communication_crc = CommunicationCRC(self.offset_bytes, self.data_bytes) if use_crc else None
        self.aliases = []
        self.parent_device = None

    def register_alias(self, alias):
        self.aliases.append(alias)

    def __str__(self):
        return (f'I2CDevice({self.address:#x},{self.offset_bytes*8},'
                f'{self.data_bytes*8},{repr(self.name)},{self.write_flags:#x})')

    def __repr__(self):
        return self.__str__()

    def __enter__(self):
        pyhslassert(not I2CDevice.s_current_device, "Other I2C device already in use")
        I2CDevice.s_current_device = self
        self.logger.debug(f'Using {self}')
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        I2CDevice.s_current_device = None
        self.logger.debug(f'Done with {self}')

    @staticmethod
    def current_device():
        pyhslassert(I2CDevice.s_current_device, "No current I2C device")
        return I2CDevice.s_current_device

    def setVerify(self, verify):
        if verify:
            self.write_flags &= ~I2C_WRITE_NO_READ_VERIFY
        else:
            self.write_flags |= I2C_WRITE_NO_READ_VERIFY

    def real_address(self):
        return self.parent_device.address if self.parent_device else self.address

    def write(self, *args, **kwargs):
        flags = self.write_flags
        for key, value in kwargs.items():
            if key not in ('noverify',):
                raise PyHslException(f'write(): invalid keyword argument: {key}')
        noverify = kwargs.get('noverify', False)
        if noverify:
            flags |= I2C_WRITE_NO_READ_VERIFY
        pyhslassert(len(args) > 0, 'write() must be invoked with at least 1 argument')
        if len(args) == 1:
            # write(((off1,data1), (off2,data2), ...)))
            pyhslassert(isinstance(args[0], collections.abc.Sequence), f'write(): parameter {args[0]} must be a sequence')
            for item in args[0]:
                pyhslassert(isinstance(item, collections.abc.Sequence), f'write(): {item} is not a collection')
                pyhslassert(len(item) == 2, f'write(): {item} should have 2 elements (offset and data)')
                Sequence.current_sequence().recorder.writeI2C(self.address, item[0], item[1], flags)

            if self.communication_crc:
                self.communication_crc.write(self.real_address(), item[0], item[1])
                for alias in self.aliases:
                    alias.communication_crc.write(self.real_address(), item[0], item[1])

        else:
            offset = args[0]
            pyhslassert(isinstance(offset, int), 'write(): first argument (offset) must be an integer')
            pyhslassert(len(args) == 2, 'usage: write(offset, data | (1,2,3,...)')
            if isinstance(args[1], collections.abc.Sequence):
                # write(offset, (a,b,c,...]))
                seq = args[1]
                pyhslassert(len(seq) > 0, 'write(): empty sequence')
                for item in seq:
                    pyhslassert(isinstance(item, int), f'write(): all elements of {seq} must be integers')
                Sequence.current_sequence().recorder.writeStreamI2C(self.address, offset, seq, flags)

                if self.communication_crc:
                    self.communication_crc.write_stream(self.real_address(), offset, seq)
                    for alias in self.aliases:
                        alias.communication_crc.write_stream(self.real_address(), offset, seq)
            else:
                # write(offset, data)
                Sequence.current_sequence().recorder.writeI2C(self.address, offset, args[1], flags)

                if self.communication_crc:
                    self.communication_crc.write(self.real_address(), offset, args[1])
                    for alias in self.aliases:
                        alias.communication_crc.write(self.real_address(), offset, args[1])
        return self

    def writeMasked(self, offset:int, data:int, mask:int, reset_value:int=0xFF):
        Sequence.current_sequence().recorder.writeMaskedI2C(self.address, offset, data, mask)

        if self.communication_crc:
            self.communication_crc.write_masked(self.real_address(), offset, reset_value, data, mask)
            for alias in self.aliases:
                alias.communication_crc.write_masked(self.real_address(), offset, reset_value, data, mask)
        return self

    def poll(self, offset:int, expectedValue:int, mask:int, intervalInUsec:int, retries:int):
        Sequence.current_sequence().recorder.pollI2C(self.address, offset, expectedValue, mask, intervalInUsec, retries)

        if self.communication_crc:
            self.communication_crc.poll(self.real_address(), offset, expectedValue, mask, intervalInUsec, retries)
            for alias in self.aliases:
                alias.communication_crc.poll(self.real_address(), offset, expectedValue, mask, intervalInUsec, retries)
        return self

    def readVerify(self, offset:int, expectedValue:int, mask:int):
        Sequence.current_sequence().recorder.readVerifyI2C(self.address, offset, expectedValue, mask)

        if self.communication_crc:
            self.communication_crc.read_verify(self.real_address(), offset, expectedValue, mask)
            for alias in self.aliases:
                alias.communication_crc.read_verify(self.real_address(), offset, expectedValue, mask)
        return self

    def readVerifyStream(self, startOffset:int, expectedValues:list):
        Sequence.current_sequence().recorder.readVerifyStreamI2C(self.address, startOffset, expectedValues)

        if self.communication_crc:
            self.communication_crc.read_stream(self.real_address(), startOffset, expectedValues)
            for alias in self.aliases:
                alias.communication_crc.read_stream(self.real_address(), startOffset, expectedValues)
        return self

    def readDiscard(self, offset:int):
        Sequence.current_sequence().recorder.readDiscardI2C(self.address, offset)

        if self.communication_crc:
            self.communication_crc.read_verify(self.real_address(), offset, 0xFF, 0x00)
            for alias in self.aliases:
                alias.communication_crc.read_verify(self.real_address(), offset, 0xFF, 0x00)
        return self

    def writeFromMemory(self, offset:int, *args):
        if len(args) == 1:
            # writeFromMemory(offset, memoryItem)
            memoryItem = args[0]
            pyhslassert(isinstance(memoryItem, MemoryBlockItem), f'{args[0]} is not a MemoryBlockItem')
            Sequence.current_sequence().recorder.writeI2CFromMemory(self.address, memoryItem.parent.address, offset, memoryItem.offset, memoryItem.size)
        elif len(args) == 3:
            # writeFromMemory(offset, memoryBlock, memoryOffset, size)
            memoryBlock = args[0]
            memoryOffset = args[1]
            size = args[2]
            pyhslassert(isinstance(memoryBlock, MemoryBlock), f'{memoryBlock} is not a MemoryBlock')
            pyhslassert(isinstance(memoryOffset, int), f'{memoryOffset} is not an integer')
            pyhslassert(isinstance(size, int), f'{size} is not an integer')

            pyhslassert(memoryOffset + size <= memoryBlock.size, f'{memoryOffset}+{size} is greater than the size of the memory block "{memoryBlock.name}" ({memoryBlock.size})')
            Sequence.current_sequence().recorder.writeI2CFromMemory(self.address, memoryBlock.address, offset, memoryOffset, size)
        else:
            raise PyHslException(f'invalid arguments: {args}')
        return self

    def readToMemory(self, offset:int, *args):
        if len(args) == 1:
            # readToMemory(offset, memoryItem)
            memoryItem = args[0]
            pyhslassert(isinstance(memoryItem, MemoryBlockItem), f'{args[0]} is not a MemoryBlockItem')

            Sequence.current_sequence().recorder.readI2CToMemory(self.address, memoryItem.parent.address, offset, memoryItem.offset, memoryItem.size)
        elif len(args) == 3:
            # readToMemory(offset, memoryBlock, memoryOffset, size)
            memoryBlock = args[0]
            memoryOffset = args[1]
            size = args[2]
            pyhslassert(isinstance(memoryBlock, MemoryBlock), f'{memoryBlock} is not a MemoryBlock')
            pyhslassert(isinstance(memoryOffset, int), f'{memoryOffset} is not an integer')
            pyhslassert(isinstance(size, int), f'{size} is not an integer')

            pyhslassert(memoryOffset + size <= memoryBlock.size, f'{memoryOffset}+{size} is greater than the size of the memory block "{memoryBlock.name}" ({memoryBlock.size})')
            Sequence.current_sequence().recorder.readI2CToMemory(self.address, memoryBlock.address, offset, memoryOffset, size)
        else:
            raise PyHslException(f'invalid arguments: {args}')
        return self

    def writeTimestampToMemory(self, *args):
        if len(args) == 1:
            # writeTimestampToMemory(memoryItem)
            memoryItem = args[0]
            pyhslassert(isinstance(memoryItem, MemoryBlockItem), f'{args[0]} is not a MemoryBlockItem')

            pyhslassert(memoryItem.size == TIMESTAMP_SIZE, f'field "{memoryItem.name}" is not {TIMESTAMP_SIZE} bytes (the size of a timestamp)')
            Sequence.current_sequence().recorder.writeTimestampToMemory(memoryItem.parent.address, memoryItem.offset)
        elif len(args) == 2:
            # writeTimestampToMemory(memoryBlock, memoryOffset)
            memoryBlock = args[0]
            memoryOffset = args[1]
            pyhslassert(isinstance(memoryBlock, MemoryBlock), f'{memoryBlock} is not a MemoryBlock')
            pyhslassert(isinstance(memoryOffset, int), f'{memoryOffset} is not an integer')

            size = TIMESTAMP_SIZE
            pyhslassert(memoryOffset + size <= memoryBlock.size, f'{memoryOffset}+{size} is greater than the size of the memory block "{memoryBlock.name}" ({memoryBlock.size})')
            Sequence.current_sequence().recorder.writeTimestampToMemory(memoryBlock.address, memoryOffset)
        else:
            raise PyHslException(f'invalid arguments: {args}')
        return self

# Encode GPIO pin address from ctrl, bank, and pin (for RCE usage)
def encodeGPIOPinAddress(ctrl: int, bank: int, pin: int) -> int:
    pyhslassert(ctrl in range(0, 256), f'Invalid controller: {ctrl}')
    pyhslassert(bank in range(0, 32), f'Invalid bank: {bank}')
    pyhslassert(pin in range(0, 8), f'Invalid pin: {pin}')
    return (ctrl << 16) | (bank << 3) | pin

class GPIOPin:

    """
    A single GPIO pin.

    Each GPIO pin has a physical name - a platform-specific identifier to the pin -
    and flags.
    """

    s_current_pin = None     # GPIO pin currently in use (inside a "with" block)
    s_all_devices = []

    __slots__ = ('logger', 'physicalName', 'name', 'flags')

    def __init__(self, physicalName:int, name:str=None, readable:bool=True, writable:bool=True):
        self.logger = logging.getLogger("pyhsl")
        self.physicalName = physicalName
        self.flags =  GPIO_PIN_READABLE if readable else 0
        self.flags |= GPIO_PIN_WRITABLE if writable else 0
        self.name = name
        GPIOPin.s_all_devices.append(self)

    def __str__(self):
        return f'GPIOPin({self.physicalName:#x},{repr(self.name)},{self.flags:#x})'

    def __repr__(self):
        return self.__str__()

    def __enter__(self):
        pyhslassert(not GPIOPin.s_current_pin, "Other GPIO device already in use")
        GPIOPin.s_current_pin = self
        self.logger.debug(f'Using {self}:')
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        GPIOPin.s_current_pin = None
        self.logger.debug(f'Done with {self}')

    @staticmethod
    def current_device():
        pyhslassert(GPIOPin.s_current_pin, "No current GPIO device")
        return GPIOPin.s_current_pin

    def write(self, value:bool):
        pyhslassert(self.flags & GPIO_PIN_WRITABLE, "GPIO pin is not writable")
        Sequence.current_sequence().recorder.writeGPIO(self.physicalName, value)
        return self

    def readVerify(self, expectedValue:bool):
        pyhslassert(self.flags & GPIO_PIN_READABLE, "GPIO pin is not readable")
        Sequence.current_sequence().recorder.readVerifyGPIO(self.physicalName, expectedValue)
        return self

    def poll(self, expectedValue:bool, intervalUsec:int, retries:int):
        pyhslassert(self.flags & GPIO_PIN_READABLE, "GPIO pin is not readable")
        Sequence.current_sequence().recorder.pollGPIO(self.physicalName, expectedValue, intervalUsec, retries)
        return self

class Fence:

    """
    A single fence.

    Each fence contains a memory address and a threshold.
    """

    s_all_fences = []

    __slots__ = ('logger', 'address', 'threshold', 'name')

    def __init__(self, address:int, threshold:int, name:str=None):
        self.logger = logging.getLogger("pyhsl")
        self.address = address
        self.threshold = threshold
        self.name = name
        Fence.s_all_fences.append(self)

    def __str__(self):
        return f'Fence({self.address:#x},{self.threshold:#x},"{self.name}")'

    def __repr__(self):
        return self.__str__()

    def wait(self, timeoutInUsec:int):
        Sequence.current_sequence().recorder.waitFence(self.address, timeoutInUsec)
        return self

    def signal(self):
        Sequence.current_sequence().recorder.signalFence(self.address)
        return self

class InternalSemaphore:

    """
    A single internal semaphore.

    Each semaphore contains an index.
    """

    s_all_semaphores = []

    __slots__ = ('logger', 'index', 'name')

    def __init__(self, index:int, name:str=None):
        self.logger = logging.getLogger("pyhsl")
        self.index = index
        self.name = name
        InternalSemaphore.s_all_semaphores.append(self)

    def __str__(self):
        return f'InternalSemaphore({self.index},"{self.name}")'

    def __repr__(self):
        return self.__str__()

    def wait(self, timeoutInUsec:int):
        Sequence.current_sequence().recorder.waitInternalSemaphore(self.index, timeoutInUsec)
        return self

    def signal(self):
        Sequence.current_sequence().recorder.signalInternalSemaphore(self.index)
        return self

class MemoryLayoutItem:
    """
    A single memory layout item.

    Each item identifies an offset and size of a field in a memory layout.
    """

    __slots__ = ('name', 'offset', 'size')

    def __init__(self, name:str, offset:int, size:int):
        self.name = name
        self.offset = offset
        self.size = size

class MemoryLayout:
    """
    A memory layout.

    A memory layout contains a list of memory layout items.
    """

    __slots__ = ('items', 'totalSize')

    def __init__(self):
        self.items = []
        self.totalSize = 0

    def add_item(self, name:str, size:int):
        if name in [item.name for item in self.items]:
            raise PyHslException(f'MemoryLayout.add_item(): item "{name}" already exists in this layout')
        self.items.append(MemoryLayoutItem(name, self.totalSize, size))
        self.totalSize += size

    def _get_cpp_field(self, item:MemoryLayoutItem):
        types = (None, 'uint8_t', 'uint16_t', None, 'uint32_t', None, None, None, 'uint64_t')
        if item.size > len(types)-1 or types[item.size] is None:
            return f'{types[1]} {item.name}[{item.size}]'
        else:
            return f'{types[item.size]} {item.name}'

    def get_cpp_definition(self, structName:str):
        return f'struct {structName} {{\n' + '\n'.join([f'    {self._get_cpp_field(item)};' for item in self.items]) + '\n};'

class MemoryBlockItem:
    """
    A single named field in a memory block.

    Each item identifies an offset and size of a field in a memory block, plus a
    reference back to its parent memory block. This allows things like
    writeMemoryToI2C(offset, block.fieldName) to work.
    """

    __slots__ = ('name', 'offset', 'size', 'parent')

    def __init__(self, name:str, offset:int, size:int, parent):
        self.name = name
        self.offset = offset
        self.size = size
        self.parent = parent

class MemoryBlock:

    """
    A single memory block.

    Each memory block contains an address and a size.
    """

    s_all_blocks = []
    s_block_count = 0

    # Sadly, we can't use __slots__ here because we need to be able to add
    # attributes at runtime.
    # __slots__ = ('logger', 'address', 'size', 'name', 'layout')

    def __init__(self, size:int, layout:MemoryLayout=None, name:str=None):
        self.logger = logging.getLogger("pyhsl")
        self.address = MemoryBlock.s_block_count
        self.size = size
        self.name = name
        self.layout = layout
        MemoryBlock.s_all_blocks.append(self)
        MemoryBlock.s_block_count += 1

        if self.layout:
            if self.layout.totalSize > self.size:
                raise PyHslException(f'MemoryBlock({self.name}): layout size {self.layout.totalSize} is greater than block size {self.size}')
            for item in self.layout.items:
                setattr(self, item.name, MemoryBlockItem(item.name, item.offset, item.size, self))

    def __str__(self):
        return f'MemoryBlock({self.address:#x},{self.size:#x},"{self.name}")'

    def __repr__(self):
        return self.__str__()


def write(*args, **kwargs):
    """
    Convenience method for writing to the current I2C device.
    """
    I2CDevice.current_device().write(*args, **kwargs)

def writeMasked(*args, **kwargs):
    """
    Convenience method for writing masked data to the current I2C device.
    """
    I2CDevice.current_device().writeMasked(*args, **kwargs)

def poll(*args, **kwargs):
    """
    Convenience method for polling the current I2C device.
    """
    I2CDevice.current_device().poll(*args, **kwargs)

def readVerify(*args, **kwargs):
    """
    Convenience method for read verifying the current I2C device.
    """
    I2CDevice.current_device().readVerify(*args, **kwargs)

def readDiscard(*args, **kwargs):
    """
    Convenience method for read discarding the current I2C device.
    """
    I2CDevice.current_device().readDiscard(*args, **kwargs)

def writeGPIO(*args, **kwargs):
    """
    Convenience method for writing to the current GPIO device.
    """
    GPIOPin.current_device().write(*args, **kwargs)

def readVerifyGPIO(*args, **kwargs):
    """
    Convenience method for read verifying the current GPIO device.
    """
    GPIOPin.current_device().readVerify(*args, **kwargs)

def pollGPIO(*args, **kwargs):
    """
    Convenience method for polling the current GPIO device.
    """
    GPIOPin.current_device().poll(*args, **kwargs)

def delay(delay_in_usec:int):
    """
    Convenience method for inserting a delay.
    """
    Sequence.current_sequence().delay(delay_in_usec)

def annotate(comment:str):
    """
    Convenience method for inserting an annotation.
    """
    Sequence.current_sequence().annotate(comment)

def readVerifyStream(*args, **kwargs):
    """
    Convenience method for read verifying stream data from the current I2C device.
    """
    I2CDevice.current_device().readVerifyStream(*args, **kwargs)

def writeFromMemory(*args, **kwargs):
    """
    Convenience method for writing from memory to the current I2C device.
    """
    I2CDevice.current_device().writeFromMemory(*args, **kwargs)

def readToMemory(*args, **kwargs):
    """
    Convenience method for reading from the current I2C device to memory.
    """
    I2CDevice.current_device().readToMemory(*args, **kwargs)

def writeTimestampToMemory(*args, **kwargs):
    """
    Convenience method for writing timestamp to memory from the current I2C device.
    """
    I2CDevice.current_device().writeTimestampToMemory(*args, **kwargs)

def waitFence(fence, timeoutInUsec:int):
    """
    Convenience method for waiting on a fence.
    """
    fence.wait(timeoutInUsec)

def signalFence(fence):
    """
    Convenience method for signaling a fence.
    """
    fence.signal()

def waitInternalSemaphore(semaphore, timeoutInUsec:int):
    """
    Convenience method for waiting on an internal semaphore.
    """
    semaphore.wait(timeoutInUsec)

def signalInternalSemaphore(semaphore):
    """
    Convenience method for signaling an internal semaphore.
    """
    semaphore.signal()

class Sequence:

    """
    An entire HSL sequence.

    The actual bytecode is kept in the HSL encoder object owned by this object.
    The proper way to use this class is to create a Sequence object in a "with" block;
    note the __enter__ and __exit__ support.  When the with block completes, the
    resulting bytecode will be optionally written to a .hslb file with the same name
    as the Sequence itself.  The bytecode will also be added to the HSLC packager.

    The static "set_hooks" method can be used to register functions that will be
    called automatically at the beginning and/or end of every Sequence.
    """

    s_current_sequence = None   # Sequence currently in use (inside a "with" block)
    s_sequences = {}            # Dictionary of all sequences created
    s_before_hook = None        # Function called upon Sequence entry
    s_after_hook = None         # Function called upon Sequence exit

    __slots__ = ('name', 'logger', 'recorder')

    def __init__(self, name:str, max_blob_size:int=0x10000):
        if Sequence.s_sequences.get(name):
            raise PyHslException(f"Sequence {name} already exists")
        self.name = name
        self.logger = ph_logger()
        self.recorder = HSLRecorder(max_blob_size)
        for device in I2CDevice.s_all_devices:
            self.recorder.addI2CDevice(device)
        for device in GPIOPin.s_all_devices:
            self.recorder.addGPIOPin(device)
        for fence in Fence.s_all_fences:
            self.recorder.addFence(fence)
        for block in MemoryBlock.s_all_blocks:
            self.recorder.addMemory(block)

    def __enter__(self):
        if Sequence.s_current_sequence:
            raise PyHslException("Another sequence is already in use")
        Sequence.s_current_sequence = self
        self.logger.debug(f'Starting sequence {self.name}:')
        if Sequence.s_before_hook:
            Sequence.s_before_hook()
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        if exc_type:
            self.logger.warning(f'Sequence "{self.name}" failed with "{exc_value}" -- discarding')
        else:
            if Sequence.s_after_hook:
                Sequence.s_after_hook()
            Sequence.s_sequences[self.name] = self
            self.recorder.recordBytecode(Configuration.s_packager, self.name)
            if Configuration.s_output_dir:
                self.recorder.writeToFile(f'{self.name}.{HSL_RAW_BYTECODE_FILE_EXTENSION}')
            self.logger.debug(f'Done with sequence {self.name}')
        Sequence.s_current_sequence = None

    @staticmethod
    def current_sequence():
        if not Sequence.s_current_sequence:
            raise PyHslException("No current sequence")
        return Sequence.s_current_sequence

    @staticmethod
    def set_hooks(before_hook, after_hook):
        pyhslassert(not before_hook or callable(before_hook), "before_hook must be callable")
        pyhslassert(not after_hook or callable(after_hook), "after_hook must be callable")
        Sequence.s_before_hook = before_hook
        Sequence.s_after_hook = after_hook

    def delay(self, delay_in_usec):
        self.recorder.delay(delay_in_usec)

    def annotate(self, comment):
        self.recorder.annotate(comment)

def ph_sequence(name:str) -> Sequence:
    return Sequence(name)

def ph_set_sequence_hooks(before_hook, after_hook) -> None:
    Sequence.set_hooks(before_hook, after_hook)


def ph_params() -> dict:
    return Configuration.s_params

def ph_logger() -> logging.Logger:
    return Configuration.s_logger
