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

#ifndef UDDF_CDI_HAL_HSLBUILDER_HPP
#define UDDF_CDI_HAL_HSLBUILDER_HPP

#include <chrono>

#include "uddf/cdi/ByteView.hpp"
#include "uddf/cdi/hal/II2CBuilder.hpp"
#include "uddf/cdi/hal/HSLBuilderStatus.hpp"

namespace hsl {
    class IHslEncoder;
}

namespace uddf::cdi {

/**
 * @brief Implements the II2CBuilder interface to construct HSL bytecode sequences.
 *
 * This class translates II2C operations into HSL commands via an IHslEncoder.
 */
class HSLBuilder : public II2CBuilder {
public:
    /**
     * @brief Constructs an HSLBuilder.
     *
     * @param[in] i2cAddress  The I2C address of the target device for HSL commands.
     * @param[in] encoder     A reference to an IHslEncoder instance that will
     *                        record the HSL bytecode.
     */
    HSLBuilder(uint16_t i2cAddress, hsl::IHslEncoder& encoder) noexcept;

    /**
     * @brief Gets the error status recorded by this builder instance.
     *
     * @retval HSLBuilderStatus The status of the last encoder operation called via this builder.
     */
    HSLBuilderStatus getLastErrorStatus() const;

    /**
     * @brief Resets the error status of the builder to HSLBuilderStatus::OK.
     */
    void resetErrorStatus() noexcept;

    void write(uint16_t offset, uint16_t data, I2CWriteFlags flags) override;
    void writeStream(uint16_t startOffset, uddf::cdi::ByteView dataView, I2CWriteFlags flags) override;
    void writeMasked(uint16_t offset, uint16_t data, uint16_t mask) override;
    void poll(uint16_t offset, uint16_t expectedValue, uint16_t mask,
                   std::chrono::microseconds interval, uint8_t retries) override;
    void readVerify(uint16_t offset, uint16_t expectedValue, uint16_t mask = 0xFFFF) override;
    void readDiscard(uint16_t offset) override;
    void readVerifyStream(uint16_t startOffset, uddf::cdi::ByteView expectedDataView) override;

private:
    /** I2C address of the target device. */
    uint16_t m_i2cAddress;
    /** Reference to the HSL encoder used to build the bytecode. */
    hsl::IHslEncoder& m_encoder;
    /** Stores the status of the last HSL encoder operation. */
    HSLBuilderStatus m_lastError = HSLBuilderStatus::OK;
};

} // namespace uddf::cdi

#endif // UDDF_CDI_HAL_HSLBUILDER_HPP
