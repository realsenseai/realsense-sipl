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

#ifndef UDDF_CDI_HAL_HSLDYNAMICSEQUENCE_HPP
#define UDDF_CDI_HAL_HSLDYNAMICSEQUENCE_HPP

#include <optional>
#include <vector>
#include <cstddef>
#include <cstdint>

#include "uddf/cdi/ByteView.hpp"
#include "uddf/cdi/hal/HSLBuilder.hpp"
#include "uddf/cdi/hal/HSLBuilderStatus.hpp"
#include "uddf/ddi/DDITypes.hpp"

namespace hsl {
    class IHslEncoder;
}

namespace uddf::cdi {

/**
 * @brief Manages the creation of an HSL sequence for multiple devices.
 */
class HSLDynamicSequence {
public:
    /**
     * @brief Constructs an HSLDynamicSequence.
     *
     * @param[in] devices      A table containing the I2C addresses of target devices.
     * @param[in] encoder      A reference to the shared IHslEncoder instance.
     */
    HSLDynamicSequence(const uddf::ddi::DeviceTable& devices, hsl::IHslEncoder& encoder);

    /**
     * @brief Retrieves the II2CBuilder for a specific device index.
     *
     * @param[in] index  The index corresponding to the device's address in the
     *                   initial DeviceTable.
     *                   Valid Range: [0 .. number of devices - 1].
     *
     * @retval II2CBuilder*  Pointer to the HSLBuilder for the requested index.
     *                       Returns nullptr if the index is out of bounds.
     */
    II2CBuilder* getI2CBuilder(size_t index);

    /**
     * @brief Retrieves the generated HSL blob if the sequence is valid.
     *
     * Checks the status of all internal builders. If all are Ok, requests the
     * final blob from the IHslEncoder and writes it into the provided buffer.
     *
     * @param[in] buffer The buffer where the HSL blob will be written.
     * @param[in] bufferSize The size of the provided buffer in bytes.
     * @param[out] blobSize On success, contains the actual size of the written blob.
     * @retval HSLBuilderStatus Ok on success, or an error status if generation fails,
     *                          the sequence is invalid, or the buffer has insufficient capacity.
     */
    HSLBuilderStatus getBlob(uint8_t buffer[], size_t const bufferSize, size_t& blobSize);

    /**
     * @brief Gets the overall status of the sequence generation.
     *
     * @retval HSLBuilderStatus The overall status, indicating success (Ok) or the
     *                          first error encountered among the builders.
     */
    HSLBuilderStatus getStatus() const;

    /**
     * @brief Resets the sequence, clearing all builders and the underlying encoder.
     *
     * After calling reset, the HSLDynamicSequence can be used to build a new
     * sequence from scratch using the same IHslEncoder instance.
     */
    void reset();

private:
    /** @brief Reference to the shared HSL encoder instance. */
    hsl::IHslEncoder& m_encoder;

    /** @brief Vector of HSLBuilders, one for each target device. */
    std::vector<HSLBuilder> m_builders;

    /** @brief Stored I2C device table for resetting builders via placement new. */
    uddf::ddi::DeviceTable m_deviceTable;
};

} // namespace uddf::cdi

#endif // UDDF_CDI_HAL_HSLDYNAMICSEQUENCE_HPP
