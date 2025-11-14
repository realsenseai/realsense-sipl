/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */

/**
 * @file chsldef.h
 *
 * C definitions for HSL bytecode.
 */

#ifndef CHSLDEF_H
#define CHSLDEF_H

#pragma pack(push, 1)

#define HSL_BYTECODE_MAJOR_VERSION (0)
#define HSL_BYTECODE_MINOR_VERSION (9)

/**
 * Flags for I2C write opcodes.
 */
enum I2CWriteFlags
{
    I2C_WRITE_NO_READ_VERIFY    = 0x01,     ///< Cannot read this register after write to verify contents
};

/**
 * Opcodes defined for an HSL stream.
 */
enum hsl_Opcode
{
    HSL_OP_NONE                      = 0, ///< No operation
    HSL_OP_I2C_DEVICE_TABLE          = 1, ///< Provide the list of I2C devices usable in this blob
    HSL_OP_USE_I2C_DEVICE            = 2, ///< Select the device (from the I2C_DEVICE_TABLE) to use for succeeding operations
    HSL_OP_I2C_TIMEOUT               = 3, ///< Set the timeout for I2C operations
    HSL_OP_WRITE_I2C                 = 4, ///< Write a series of bytes to I2C
    HSL_OP_WRITE_VERIFY_I2C          = 5, ///< Write a series of bytes to I2C with readback verification
    HSL_OP_READ_VERIFY_I2C           = 6, ///< Read I2C register and verify against expected value (with mask)
    HSL_OP_READ_VERIFY_STREAM_I2C    = 7, ///< Read a series of bytes from I2C and verify against expected values
    HSL_OP_MODIFY_I2C                = 8, ///< Read I2C register, then replace mask bits with write value and write back (read-modify-write)
    HSL_OP_POLL_I2C                  = 9, ///< Poll I2C register
    HSL_OP_GPIO_PIN_TABLE            = 10, ///< Provide the list of GPIO pins usable in this blob
    HSL_OP_USE_GPIO_PIN              = 11, ///< Select the pin (from the GPIO_PIN_TABLE) to use for succeeding operations
    HSL_OP_WRITE_GPIO                = 12, ///< Set value of a GPIO pin
    HSL_OP_READ_VERIFY_GPIO          = 13, ///< Read GPIO pin and verify against expected value
    HSL_OP_POLL_GPIO                 = 14, ///< Poll GPIO pin
    HSL_OP_FENCE_TABLE               = 15, ///< Provide the list of fences usable in this blob
    HSL_OP_USE_FENCE                 = 16, ///< Select the fence (from the FENCE_TABLE) to use for succeeding semaphore operations
    HSL_OP_USE_INTERNAL_SEMAPHORE    = 17, ///< Select the internal semaphore to use for succeeding semaphore operations
    HSL_OP_WAIT_SEMAPHORE            = 18, ///< Wait on semaphore/fence
    HSL_OP_SIGNAL_SEMAPHORE          = 19, ///< Signal semaphore/fence
    HSL_OP_MEMORY_TABLE              = 20, ///< Provide the list of memory regions usable in this blob
    HSL_OP_USE_MEMORY                = 21, ///< Select the memory region (from the MEMORY_TABLE) to use for succeeding operations
    HSL_OP_USE_SCRATCH_MEMORY        = 22, ///< Select the internal scratch memory region to use for succeeding operations
    HSL_OP_WRITE_I2C_FROM_MEMORY     = 23, ///< Write a series of bytes to I2C from the current memory region
    HSL_OP_READ_I2C_TO_MEMORY        = 24, ///< Read a series of bytes from I2C to the current memory region
    HSL_OP_WRITE_TIMESTAMP_TO_MEMORY = 25, ///< Write the current timestamp to the current memory region
    HSL_OP_DELAY                     = 26, ///< Delay processing for specified duration
    HSL_OP_ANNOTATE                  = 27, ///< "Comment" in stream
    HSL_OP_I2C_XFER                  = 28, ///< Perform an I2C transfer (RCE only)
    HSL_OP_CALLBACK                  = 29, ///< Call a callback function (RCE only)
    HSL_OP_RELEASE                   = 30, ///< Release resources (RCE only)
};

/**
 * Flags for I2C Device Table entries.
 */
enum I2CDeviceTableFlags
{
    I2C_DEVICE_AUTO_RETRY       = 0x01,     ///< Always retry failed I2C operations for this device
    I2C_DEVICE_10_BIT_ADDRESS   = 0x02,     ///< Device uses 10-bit addressing (else 7-bit)
};

#define MAX_I2C_DEVICE_ADDRESS_7BIT  0x7F
#define MAX_I2C_DEVICE_ADDRESS_10BIT 0x3FF

/**
 * Single item in the I2C device table.
 * The I2C device table lists all I2C devices accessed by the HSL stream.
 */
typedef struct
{
    uint16_t address;       ///< I2C address of this device
    uint8_t  flags;         ///< Information about this device (I2C_DEVICE_* flags)
    uint8_t  offsetWidth;   ///< Width of register offsets for this device, in bytes
    uint8_t  dataWidth;     ///< Width of data for this device, in bytes
} hsl_I2CDeviceTableEntry;

#define I2C_DEVICE_TABLE_ENTRY_SIZE (sizeof(hsl_I2CDeviceTableEntry))

/**
 * Flags for GPIO Pin Table entries.
 */
enum GpioPinTableFlags
{
    GPIO_PIN_READABLE = 0x01,   ///< GPIO pin can be read by CPU
    GPIO_PIN_WRITABLE = 0x02,   ///< GPIO pin can be driven by CPU
};

/**
 * Platform-specific identifier for a GPIO pin.
 * On QNX, this is an index into the GPIO pin descriptor array in DT under
 * tegra/gpios (for the appropriate device block).
 */
typedef uint32_t GpioPhysicalName;

#define GPIO_INVALID_PHYSICAL_NAME 0xFFFFFFFF

/**
 * Single item in the GPIO pin table.
 * The GPIO pin table lists all GPIO pins accessed by the HSL stream.
 */
typedef struct
{
    GpioPhysicalName physicalName;     ///< Physical address (OS-specific) of this GPIO pin
    uint8_t          flags;            ///< Information about this device (GPIO_DEVICE_* flags)
} hsl_GpioPinTableEntry;

#define GPIO_PIN_TABLE_ENTRY_SIZE (sizeof(hsl_GpioPinTableEntry))


/**
 * Single item in the fence table.
 * The fence table lists all fences accessed by the HSL stream.
 */
typedef struct
{
    uint64_t address;
    uint64_t threshold;
} hsl_FenceTableEntry;

#define FENCE_TABLE_ENTRY_SIZE (sizeof(hsl_FenceTableEntry))

/**
 * Flags for Memory Table entries.
 */
enum MemoryTableFlags
{
    MEMORY_TABLE_READABLE = 0x01,   ///< Memory block can be read by HSL operations
    MEMORY_TABLE_WRITABLE = 0x02,   ///< Memory block can be written by HSL operations
};

typedef struct
{
    uint64_t address;
    uint32_t length;
    uint8_t flags;
} hsl_MemoryTableEntry;

#define MEMORY_TABLE_ENTRY_SIZE (sizeof(hsl_MemoryTableEntry))


typedef struct
{
    uint8_t opcode;         ///< One of the HSL_OP enum values defined above
    uint8_t operationSize;  ///< Number of bytes in this operation (including the size of this struct)
} hsl_OperationCommon;

#define SIZE_HSL_OP_USE_I2C_DEVICE  (3) ///< Size (in bytes) of the HSL_OP_USE_I2C_DEVICE instruction
#define SIZE_HSL_OP_USE_GPIO_PIN    (3) ///< Size (in bytes) of the HSL_OP_USE_GPIO_PIN instruction
#define SIZE_HSL_OP_USE_FENCE       (3) ///< Size (in bytes) of the HSL_OP_USE_FENCE instruction
#define SIZE_HSL_OP_USE_INTERNAL_SEMAPHORE (3) ///< Size (in bytes) of the HSL_OP_USE_INTERNAL_SEMAPHORE instruction
#define SIZE_HSL_OP_USE_MEMORY      (3) ///< Size (in bytes) of the HSL_OP_USE_MEMORY instruction
#define SIZE_HSL_OP_USE_MEMORY_SCRATCH (2) ///< Size (in bytes) of the HSL_OP_USE_MEMORY_SCRATCH instruction

#define BLOB_MAGIC          0x314C5348

/**
 * Layout of the header on an HSL blob.
 */
typedef struct
{
    uint16_t operationCount       {0};  ///< Number of operations in stream
    uint16_t i2cDeviceTableOffset {0};  ///< Offset of first I2C device table entry (from beginning of blob)
    uint16_t i2cDeviceTableCount  {0};  ///< Number of I2C device table entries
    uint16_t gpioPinTableOffset   {0};  ///< Offset of first GPIO pin table entry (from beginning of blob)
    uint16_t gpioPinTableCount    {0};  ///< Number of GPIO pin table entries
    uint16_t fenceTableOffset     {0};  ///< Offset of first fence table entry (from beginning of blob)
    uint16_t fenceTableCount      {0};  ///< Number of fence table entries
    uint16_t memoryTableOffset    {0};  ///< Offset of first memory table entry (from beginning of blob)
    uint16_t memoryTableCount     {0};  ///< Number of memory table entries
} hsl_BlobHeader;
static_assert(sizeof(hsl_BlobHeader) == 18, "hsl_BlobHeader size is not 18 bytes");

#pragma pack(pop)

#endif // CHSLDEF_H
