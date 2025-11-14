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
 * @file IHslDecoder.h
 *
 * The HSL bytecode decoder interface.
 * Objects with this interface parse HSL bytecode, making callbacks to a provided interface
 * as operations are encountered.
 */

#ifndef IHSLDECODER_H_DEFINED
#define IHSLDECODER_H_DEFINED

#include <cstdint>
#include <cstddef>
#include <memory>
#include "hsldef.h"

namespace hsl
{

/**
 * Interface for HSL bytecode decoder callbacks.
 */
class IDecoderCallbacks
{
public:

    /**
     * Sentinel value used as the memory block index for operations accessing
     * internal scratch memory (instead of an entry in the memory block table).
     */
    static constexpr uint8_t SCRATCH_MEMORY_INDEX = 0xFE;

    IDecoderCallbacks& operator=(const IDecoderCallbacks&) = delete;
    IDecoderCallbacks& operator=(const IDecoderCallbacks&&) = delete;
    IDecoderCallbacks(IDecoderCallbacks&) = delete;
    IDecoderCallbacks(IDecoderCallbacks&&) = delete;

    /**
     * Provide basic information about the blob.
     * This will be the first callback invoked, and it will be invoked only once.
     *
     * @param[in] operationCount The number of operations in the blob.
     */
    virtual void blobInfo(size_t operationCount) = 0;

    /**
     * Provide the I2C device table.
     * This callback may not be invoked (if there are no I2C operations);
     * if it is invoked, it will be only once, and before any I2C operation callbacks occur.
     *
     * @param[in] i2cDeviceTable The table of I2C devices referenced in the blob.
     * Note that the other callbacks refer to devices by their indices into this table.
     * @param[in] i2cDeviceTableCount The number of devices in the I2C device table.
     *
     * @return @c false if playback should exit immediately with an error,
     * @c true if playback should continue.
     */
    virtual bool i2cDeviceTable(hsl_I2CDeviceTableEntry const* i2cDeviceTable, size_t i2cDeviceTableCount) = 0;

    /**
     * Provide the GPIO pin table.
     * This callback may not be invoked (if there are no GPIO operations);
     * if it is invoked, it will be only once, and before any GPIO operation callbacks occur.
     *
     * @param[in] gpioPinTable The table of GPIO pins referenced in the blob.
     * Note that the other callbacks refer to pins by their indices into this table.
     * @param[in] gpioPinTableCount The number of pins in the GPIO pin table.
     *
     * @return @c false if playback should exit immediately with an error,
     * @c true if playback should continue.
     */
    virtual bool gpioPinTable(hsl_GpioPinTableEntry const* gpioPinTable, size_t gpioPinTableCount) = 0;

    /**
     * Provide the fence table.
     * This callback may not be invoked (if there are no fence operations);
     * if it is invoked, it will be only once, and before any fence operation callbacks occur.
     *
     * @param[in] fenceTable The table of fences referenced in the blob.
     * Note that the other callbacks refer to fences by their indices into this table.
     * @param[in] fenceTableCount The number of fences in the fence table.
     *
     * @return @c false if playback should exit immediately with an error,
     * @c true if playback should continue.
     */
    virtual bool fenceTable(hsl_FenceTableEntry const* fenceTable, size_t fenceTableCount) = 0;

    /**
     * Provide the memory table.
     * This callback may not be invoked (if there are no memory operations);
     * if it is invoked, it will be only once, and before any memory operation callbacks occur.
     *
     * @param[in] memoryTable The table of memory regions referenced in the blob.
     * Note that the other callbacks refer to memory regions by their indices into this table.
     * @param[in] memoryTableCount The number of memory regions in the memory table.
        *
     * @return @c false if playback should exit immediately with an error,
     * @c true if playback should continue.
     */
    virtual bool memoryTable(hsl_MemoryTableEntry const* memoryTable, size_t memoryTableCount) = 0;

    /**
     * Delay execution of the sequence by @c delayUsec microseconds.
     *
     * @return @c false if playback should exit immediately with an error,
     * @c true if playback should continue.
     */
    virtual bool delay(uint32_t delayUsec) = 0;

    /**
     * The stream author inserted a text annotation at this point;
     * no particular action is required, although displaying/logging the annotation
     * may be appropriate.
     *
     * @return @c false if playback should exit immediately with an error,
     * @c true if playback should continue.
     */
    virtual bool annotate(std::string const& comment) = 0;

    /**
     * Write a value to an I2C device.
     *
     * @param[in] deviceIndex The device table index of the I2C device to be written.
     * @param[in] offset The register offset to write.
     * @param[in] data The data to write.
     * Note that the device table entry at index @c deviceIndex
     * contains flags (I2C_DEVICE_*) that specify the width of both offset and data (8 or 16 bits).
     * @param[in] i2cWriteFlags Flags controlling the write operation (I2C_WRITE_*),
     * which are defined by the I2CWriteFlags enum type.
     *
     * @return @c false if playback should exit immediately with an error,
     * @c true if playback should continue.
     */
    virtual bool writeI2C(uint8_t deviceIndex, uint16_t offset, uint16_t data, uint8_t i2cWriteFlags) = 0;

    /**
     * Write a sequence of bytes to an I2C device.
     *
     * @param[in] deviceIndex The device table index of the I2C device to be written.
     * @param[in] startOffset The register offset of the first byte to be written.
     * @param[in] bytes The bytes to be written to the device.
     * @param[in] byteCount The number of bytes in the @c bytes array.
     * @param[in] i2cWriteFlags Flags controlling the write operation (I2C_WRITE_*),
     * which are defined by the I2CWriteFlags enum type.
     *
     * @return @c false if playback should exit immediately with an error,
     * @c true if playback should continue.
     */
    virtual bool writeStreamI2C(uint8_t deviceIndex, uint16_t startOffset, uint8_t const* bytes, uint16_t byteCount, uint8_t i2cWriteFlags) = 0;

    /**
     * Write one value to an I2C device using a mask.
     * This will generate a read-modify-write sequence.
     *
     * @param[in] deviceIndex The device table index of the I2C device to be written.
     * @param[in] offset The register offset to be written.
     * Note that the device table entry at index @c deviceIndex
     * contains flags (I2C_DEVICE_*) that specify the width of both offset and data (8 or 16 bits).
     * @param[in] data The data to be written to @c offset.
     * @param[in] mask The bit mask to be used.
     * The final value in the register should be <tt>(data & mask) | (previous-data) & ~mask)</tt>.
     *
     * @return @c false if playback should exit immediately with an error,
     * @c true if playback should continue.
     */
    virtual bool writeMaskedI2C(uint8_t deviceIndex, uint16_t offset, uint16_t data, uint16_t mask) = 0;

    /**
     * Poll an I2C register until a particular value is seen.
     *
     * @param[in] deviceIndex The device table index of the I2C device to be polled.
     * @param[in] offset The register offset to poll.
     * Note that the device table entry at index @c deviceIndex
     * contains flags (I2C_DEVICE_*) that specify the width of both offset and data (8 or 16 bits).
     * @param[in] expectedValue The value that the polling operation is waiting for.
     * @param[in] mask The bit mask to be used.
     * The polling should continue until <tt>(data-read & mask) == expectedValue</tt>.
     * @param[in] intervalUsec The interval (in microseconds) between reads.
     * @param[in] retries The maximum number of retries allowed for the poll operation.
     * If @c expectedValue has not been seen after this many retries,
     * the poll operation fails.
     *
     * @return @c false if playback should exit immediately with an error,
     * @c true if playback should continue.
     */
    virtual bool pollI2C(uint8_t deviceIndex, uint16_t offset, uint16_t expectedValue, uint16_t mask, uint32_t intervalUsec, uint8_t retries) = 0;

    /**
     * Read an I2C register and report an error if its value does not match an expected value.
     *
     * @param[in] deviceIndex The device table index of the I2C device to be read.
     * @param[in] offset The register offset to be read.
     * Note that the device table entry at index @c deviceIndex
     * contains flags (I2C_DEVICE_*) that specify the width of both offset and data (8 or 16 bits).
     * @param[in] expectedValue The value that is expected to be read from @c offset.
     * @param[in] mask The bit mask to be used.
     * This operation should fail if <tt>(data-read & mask) != expectedValue</tt>.
     *
     * @return @c false if playback should exit immediately with an error,
     * @c true if playback should continue.
     */
    virtual bool readVerifyI2C(uint8_t deviceIndex, uint16_t offset, uint16_t expectedValue, uint16_t mask) = 0;

    /**
     * Read an I2C register but don't do anything with the result.
     *
     * @param[in] deviceIndex The device table index of the I2C device to be read.
     * @param[in] offset The register offset to be read.
     * Note that the device table entry at index @c deviceIndex
     * contains flags (I2C_DEVICE_*) that specify the width of the offset (8 or 16 bits).
     *
     * @return @c false if playback should exit immediately with an error,
     * @c true if playback should continue.
     */
    virtual bool readDiscardI2C(uint8_t deviceIndex, uint16_t offset) = 0;

    /**
     * Read a series of bytes from an I2C device and verify against expected values.
     *
     * @param[in] deviceIndex The device table index of the I2C device to be read.
     * @param[in] startOffset The register offset of the first byte to be read.
     * @param[in] expectedValues The values that are expected to be read from @c startOffset and successive addresses.
     * <tt>expectedValues[0]</tt> will be compared to the value at <tt>startOffset</tt>,
     * <tt>expectedValues[1]</tt> will be compared to the value at <tt>startOffset+1</tt>,
     * and so on.
     * @param[in] byteCount The number of bytes to be read (and the size of the @c expectedValues array).
     *
     * @return @c false if playback should exit immediately with an error,
     * @c true if playback should continue.
     */
    virtual bool readVerifyStreamI2C(uint8_t deviceIndex, uint16_t startOffset, uint8_t const* expectedValues, uint16_t byteCount) = 0;

    /**
     * Set the value of a GPIO pin.
     *
     * @param[in] pinIndex The GPIO pin table index of the pin to be set.
     * @param[in] value @c false means to pull the pin low; @c true means to pull it high.
     *
     * @return @c false if playback should exit immediately with an error,
     * @c true if playback should continue.
     */
    virtual bool writeGPIO(uint8_t pinIndex, bool value) = 0;

    /**
     * Read a GPIO pin and report an error if its value does not match an expected value.
     *
     * @param[in] pinIndex The GPIO pin table index of the pin to be read.
     * @param[in] expectedValue @c false means that the pin is expected to be low;
     * @c true means that it's expected to be high.
     *
     * @return @c false if playback should exit immediately with an error,
     * @c true if playback should continue.
     */
    virtual bool readVerifyGPIO(uint8_t pinIndex, bool expectedValue) = 0;

    /**
     * Poll a GPIO pin until an expected value is seen.
     *
     * @param[in] pinIndex The GPIO pin table index of the pin to be read.
     * @param[in] expectedValue @c false means that the pin is expected to be low;
     * @c true means that it's expected to be high.
     * @param[in] intervalUsec The interval (in microseconds) between reads.
     * @param[in] retries The maximum number of retries allowed for the poll operation.
     * If @c expectedValue has not been seen after this many retries,
     * the poll operation fails.
     *
     * @return @c false if playback should exit immediately with an error,
     * @c true if playback should continue.
     */
    virtual bool pollGPIO(uint8_t pinIndex, bool expectedValue, uint32_t intervalUsec, uint8_t retries) = 0;

    /**
     * Wait on a fence.
     *
     * @param[in] fenceIndex The fence table index of the fence.
     * @param[in] timeoutUsec The timeout (in microseconds) of the wait operation.
     * If the fence has not been signaled within this period,
     * the wait operation fails.
     *
     * @return @c false if playback should exit immediately with an error,
     * @c true if playback should continue.
     */
    virtual bool waitFence(uint8_t fenceIndex, uint32_t timeoutUsec) = 0;

    /**
     * Signal a fence.
     *
     * @param[in] fenceIndex The fence table index of the fence.
     *
     * @return @c false if playback should exit immediately with an error,
     * @c true if playback should continue.
     */
    virtual bool signalFence(uint8_t fenceIndex) = 0;

    /**
     * Wait on an internal semaphore.
     *
     * @param[in] semIndex The index of the internal semaphore.
     * @param[in] timeoutUsec The timeout (in microseconds) of the wait operation.
     * If the semaphore has not been signaled within this period,
     * the wait operation fails.
     *
     * @return @c false if playback should exit immediately with an error,
     * @c true if playback should continue.
     */
    virtual bool waitInternalSemaphore(uint8_t semIndex, uint32_t timeoutUsec) = 0;

    /**
     * Signal an internal semaphore.
     *
     * @param[in] semIndex The index of the internal semaphore.
     *
     * @return @c false if playback should exit immediately with an error,
     * @c true if playback should continue.
     */
    virtual bool signalInternalSemaphore(uint8_t semIndex) = 0;

    /**
     * Write bytes to I2C from a memory block.
     *
     * @param[in] deviceIndex The device table index of the I2C device to be written.
     * @param[in] memoryIndex The memory table index of the memory block providing the data.
     * @param[in] i2cOffset The register offset of the first byte to be written.
     * @param[in] memOffset The offset in the memory block of the first byte of data to be read.
     * @param[in] byteCount The number of bytes to be copied from memory to I2C.
     *
     * @return @c false if playback should exit immediately with an error,
     * @c true if playback should continue.
     */
    virtual bool writeI2CFromMemory(uint8_t deviceIndex, uint8_t memoryIndex, uint16_t i2cOffset, uint16_t memOffset, uint16_t byteCount) = 0;

    /**
     * Read bytes from I2C into a memory block.
     *
     * @param[in] deviceIndex The device table index of the I2C device providing the data.
     * @param[in] memoryIndex The memory table index of the memory block to be written.
     * @param[in] i2cOffset The register offset of the first byte to be read.
     * @param[in] memOffset The offset in the memory block of the first byte of data to be written.
     * @param[in] byteCount The number of bytes to be copied from I2C to memory.
     *
     * @return @c false if playback should exit immediately with an error,
     * @c true if playback should continue.
     */
    virtual bool readI2CToMemory(uint8_t deviceIndex, uint8_t memoryIndex, uint16_t i2cOffset, uint16_t memOffset, uint16_t byteCount) = 0;

    /**
     * Write the current timestamp into a memory block.
     *
     * The timestamp is 8 bytes long and represents the current time when the operation
     * is executed. The specific timestamp source (e.g., monotonic clock, system clock)
     * is implementation-dependent but should provide sufficient precision for timing
     * measurements and synchronization purposes.
     *
     * @param[in] memoryIndex The memory table index of the memory block to be written.
     * @param[in] memOffset The offset in the memory block of the first byte of data to be written.
     *
     * @return @c false if playback should exit immediately with an error,
     * @c true if playback should continue.
     */
    virtual bool writeTimestampToMemory(uint8_t memoryIndex, uint32_t memOffset) = 0;

    /**
     * Player encountered an unknown opcode.
     *
     * @param[in] opcode The value of the unknown opcode.
     *
     * @return @c false if playback should exit immediately with an error,
     * @c true if playback should continue.
     */
    virtual bool unknownOpcode(uint16_t opcode) = 0;

protected:

    IDecoderCallbacks() = default;
    virtual ~IDecoderCallbacks() = default;
};

/**
 * Interface for HSL bytecode decoder.
 */
class IHslDecoder {
public:
    virtual ~IHslDecoder() = default;

    IHslDecoder(const IHslDecoder&) = delete;
    IHslDecoder(IHslDecoder&&) = delete;
    IHslDecoder& operator=(const IHslDecoder&) = delete;
    IHslDecoder& operator=(IHslDecoder&&) = delete;

    /**
     * Start a bytecode sequence.
     *
     * @param[in] callbacks An object that implements the @c IDecoderCallbacks interface.
     * Methods in that interface will be called, corresponding to the operations in
     * the blob.
     *
     * @returns return nullptr on success, otherwise a static string of the error.
     */
    virtual char const * sequenceStart(IDecoderCallbacks& callbacks) = 0;

    /**
     * Execute a some bytecode.
     *
     * @param[in] bytecode This is the bytecode data to execute.
     * @param[in] bytes    This is the the number of bytes in the bytecode data.
     *
     * @returns return nullptr on success, otherwise a static string of the error.
     */
    virtual char const * sequenceBytecode(uint8_t const bytecode[], size_t bytes) = 0;

    /**
     * End of a bytecode sequence.
     *
     * @returns return nullptr on success, otherwise a static string of the error.
     */
    virtual char const * sequenceEnd() = 0;

    /**
     * Traverse all operations in the blob, making one callback for each operation
     * encountered (in order).
     *
     * @param[in] callbacks An object that implements the @c IDecoderCallbacks interface.
     * Methods in that interface will be called, corresponding to the operations in
     * the blob.
     *
     * @return false if the stream was corrupted, or if one of the callbacks returned
     * false (signifying a desire to cease traversal).
     */
    virtual bool playback(IDecoderCallbacks& callbacks) = 0;

    static std::unique_ptr<IHslDecoder> createDecoder(uint8_t const * blob, size_t blobSize);

protected:
    IHslDecoder() = default;
};

} // namespace hsl

#endif // IHSLDECODER_H_DEFINED
