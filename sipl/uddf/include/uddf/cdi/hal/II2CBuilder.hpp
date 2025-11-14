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

#ifndef UDDF_CDI_HAL_II2CBUILDER_HPP
#define UDDF_CDI_HAL_II2CBUILDER_HPP

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "uddf/cdi/ByteView.hpp"

namespace uddf::cdi {

/**
 * @brief Flags for I2C write operations.
 */
enum class I2CWriteFlags : uint8_t {
    /**
     * @brief No special flags.
     */
    NONE = 0x00U,
    /**
     * @brief Indicates that a register cannot be read back after a write to verify its contents.
     */
    NO_READ_VERIFY = 0x01U,
};

/**
 * @brief Interface for building a sequence of I2C commands.
 */
class II2CBuilder {
public:
    /**
     * @brief Virtual destructor.
     */
    virtual ~II2CBuilder() = default;

    /**
     * @brief Adds a write operation to the I2C command sequence.
     *
     * @param[in] offset  The register offset to write to.
     * @param[in] data    The data to write. The width of this data is defined by
     *                    device configuration.
     * @param[in] flags   Flags for the write operation.
     */
    virtual void write(uint16_t offset, uint16_t data, I2CWriteFlags flags = I2CWriteFlags::NONE) = 0;

    /**
     * @brief Adds a stream write operation to the I2C command sequence.
     *
     * @param[in] startOffset  The starting register offset for the stream write.
     * @param[in] dataView     A view of bytes to write.
     * @param[in] flags        Flags for the stream write operation.
     */
    virtual void writeStream(uint16_t startOffset, ByteView dataView, I2CWriteFlags flags = I2CWriteFlags::NONE) = 0;

    /**
     * @brief Adds a masked write operation to the I2C command sequence.
     *
     * This involves a read-modify-write operation:
     * (current_value & ~mask) | (data & mask).
     *
     * @param[in] offset  The register offset to write to.
     * @param[in] data    The data to write (bits outside the mask are ignored).
     * @param[in] mask    The mask to apply. Bits set in the mask indicate which
     *                    bits in the register to modify.
     */
    virtual void writeMasked(uint16_t offset, uint16_t data, uint16_t mask) = 0;

    /**
     * @brief Adds a poll operation to the I2C command sequence.
     *
     * @param[in] offset         The register offset to poll.
     * @param[in] expectedValue  The value expected to be read (after masking).
     * @param[in] mask           The mask to apply to the read value before comparing
     *                           with expectedValue.
     * @param[in] interval       The polling interval.
     * @param[in] retries        The maximum number of polling attempts.
     */
    virtual void poll(uint16_t offset, uint16_t expectedValue, uint16_t mask,
                           std::chrono::microseconds interval, uint8_t retries) = 0;

    /**
     * @brief Adds a read and verify operation to the I2C command sequence.
     *
     * @param[in] offset         The register offset to read from.
     * @param[in] expectedValue  The value expected to be read (after masking).
     * @param[in] mask           The mask to apply to the read value before comparing.
     *                           Defaults to 0xFFFF (all bits).
     */
    virtual void readVerify(uint16_t offset, uint16_t expectedValue, uint16_t mask = 0xFFFF) = 0;

    /**
     * @brief Adds a read and discard operation to the I2C command sequence.
     *
     * @param[in] offset  The register offset to read from.
     */
    virtual void readDiscard(uint16_t offset) = 0;

    /**
     * @brief Adds a stream read and verify operation to the I2C command sequence.
     *
     * @param[in] startOffset       The starting register offset for the stream read.
     * @param[in] expectedDataView  A view of expected bytes.
     */
    virtual void readVerifyStream(uint16_t startOffset, ByteView expectedDataView) = 0;
};

} // namespace uddf::cdi

#endif // UDDF_CDI_HAL_II2CBUILDER_HPP
