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
 * @file IHslEncoder.h
 *
 * The HSL bytecode encoder interface.
 * Objects with this interface record HSL bytecode that represents
 * the sequence of operations invoked.
 */

#ifndef IHSLENCODER_H_DEFINED
#define IHSLENCODER_H_DEFINED

#include <cstdint>
#include <cstddef>
#include <memory>
#include <string>
#include "hsldef.h"

namespace hsl {

/**
 * Callback interface for debugging.  If any callback returns false, this
 * will cause the encoder to fail.
 */
class IEncoderDebugHooks
{
public:
    /**
     * Called when a new sequence starts.
     */
    virtual bool sequence_start() = 0;

    /**
     * Called for each bytecode.
     */
    virtual bool sequence_bytecode(uint8_t const *bytecode, size_t bytes) = 0;

    /**
     * Called when a sequence ends.
     */
    virtual bool sequence_end() = 0;
};

class IHslEncoder {

public:

    enum class EncoderResult {
        OK = 0,
        BAD_PARAMS = 1,
        OUT_OF_SPACE = 2,
        NOT_IMPLEMENTED = 3,
        BAD_STATE = 4,
        DEBUG_FAILURE = 5,
    };

    virtual ~IHslEncoder() = default;

    IHslEncoder(const IHslEncoder&) = delete;
    IHslEncoder(IHslEncoder&&) = delete;
    IHslEncoder& operator=(const IHslEncoder&) = delete;
    IHslEncoder& operator=(IHslEncoder&&) = delete;

    virtual EncoderResult addI2CDevice(uint16_t address, uint8_t flags, uint8_t offsetWidth, uint8_t dataWidth) = 0;
    virtual EncoderResult addGPIOPin(GpioPhysicalName physicalName, uint8_t flags) = 0;
    virtual EncoderResult addFence(uint64_t address, uint64_t threshold) = 0;
    virtual EncoderResult addMemory(uint64_t address, uint32_t length) = 0;
    virtual EncoderResult delay(uint32_t delayUsec) = 0;
    virtual EncoderResult annotate(std::string const& comment) = 0;
    virtual EncoderResult writeI2C(uint16_t device, uint16_t offset, uint16_t data, uint8_t flags) = 0;
    virtual EncoderResult writeStreamI2C(uint16_t device, uint16_t startOffset, uint8_t const* bytes, uint16_t byteCount, uint8_t flags) = 0;
    virtual EncoderResult writeMaskedI2C(uint16_t device, uint16_t offset, uint16_t data, uint16_t mask) = 0;
    virtual EncoderResult pollI2C(uint16_t device, uint16_t offset, uint16_t expectedValue, uint16_t mask, uint32_t intervalUsec, uint8_t retries) = 0;
    virtual EncoderResult readVerifyI2C(uint16_t device, uint16_t offset, uint16_t expectedValue, uint16_t mask=0xFFFF) = 0;
    virtual EncoderResult readDiscardI2C(uint16_t device, uint16_t offset) = 0;
    virtual EncoderResult readVerifyStreamI2C(uint16_t device, uint16_t startOffset, uint8_t const* expectedValues, uint16_t byteCount) = 0;
    virtual EncoderResult writeGPIO(GpioPhysicalName physicalName, bool value) = 0;
    virtual EncoderResult readVerifyGPIO(GpioPhysicalName physicalName, bool expectedValue) = 0;
    virtual EncoderResult pollGPIO(GpioPhysicalName physicalName, bool expectedValue, uint32_t intervalUsec, uint8_t retries) = 0;
    virtual EncoderResult waitFence(uint64_t address, uint32_t timeoutUsec) = 0;
    virtual EncoderResult signalFence(uint64_t address) = 0;
    virtual EncoderResult waitInternalSemaphore(size_t semaphoreIndex, uint32_t timeoutUsec) = 0;
    virtual EncoderResult signalInternalSemaphore(size_t semaphoreIndex) = 0;
    virtual EncoderResult writeI2CFromMemory(uint16_t device, uint64_t memAddress, uint16_t i2cOffset, uint16_t memOffset, uint16_t byteCount) = 0;
    virtual EncoderResult readI2CToMemory(uint16_t device, uint64_t memAddress, uint16_t i2cOffset, uint16_t memOffset, uint16_t byteCount) = 0;
    virtual EncoderResult writeTimestampToMemory(uint64_t memAddress, uint32_t memOffset) = 0;
    virtual EncoderResult produceBlob(uint8_t buffer[], size_t const bufferSize, size_t& blobSize) = 0;
    virtual void reset() = 0;
    virtual size_t getBlobSize() const = 0;
    virtual size_t getOperationCount() const = 0;
    virtual uint8_t getMajorVersion() const = 0;
    virtual uint8_t getMinorVersion() const = 0;

    static std::unique_ptr<IHslEncoder> createEncoder(size_t streamSize, std::shared_ptr<IEncoderDebugHooks> debugHook = nullptr);

protected:

    IHslEncoder() = default;
};

} // namespace hsl

#endif // IHSLENCODER_H_DEFINED
