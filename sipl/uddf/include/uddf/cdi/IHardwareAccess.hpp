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

#ifndef UDDF_CDI_IHARDWAREACCESS_HPP
#define UDDF_CDI_IHARDWAREACCESS_HPP

#include <memory>

#include "uddf/cdi/hal/HSLDynamicSequence.hpp"
#include "uddf/cdi/hal/HSLStaticSequence.hpp"

#include "uddf/cdi/ByteView.hpp"
#include "uddf/cdi/HSLResult.hpp"

namespace uddf::cdi {

/**
 * @brief Interface for hardware access (through HSL sequences and I2C reads).
 *
 * This interface provides access to hardware resources using HSL sequences.
 * It allows for dynamic and static HSL sequence submission, as well as
 * "raw" (non-HSL)I2C read operations, which are not part of the HSL specification.
 */
class IHardwareAccess {
public:
    /**
     * @brief Virtual destructor.
     */
    virtual ~IHardwareAccess() = default;

    /**
     * @brief Acquire a HSL sequence object for building sequences at runtime.
     *
     * @return A reference to the HSLDynamicSequence.
     */
    virtual HSLDynamicSequence& GetDynamicSequence() = 0;

    /**
     * @brief Submits a dynamic HSL sequence.

     * @param[in] sequence A reference to the HSLDynamicSequence object.
     * @return HSLResult object indicating the success or failure of the operation.
     */
    virtual HSLResult SubmitSequence(HSLDynamicSequence& sequence) = 0;

    /**
     * @brief Submits a pre-defined static HSL sequence.
     *
     * @tparam N The size of the static HSL sequence data.

     * @param[in] sequence The HSLStaticSequence object containing the data.
     * @return HSLResult object indicating the success or failure of the operation.
     */
    template <size_t N>
    HSLResult SubmitSequence(const HSLStaticSequence<N>& sequence) {
        return DoSubmitStaticSequence(sequence.getBlob());
    }

    /**
     * @brief Read bytes from an I2C device.
     *
     * @param[in]  deviceIndex The index of the I2C device in the driver's device table.
     * @param[in]  startOffset The starting offset in the I2C device.
     * @param[in]  length The number of bytes to read.
     * @param[out] buffer The buffer to store the read bytes.
     * @return true if the read was successful, false otherwise.
     */
    virtual bool ReadI2C(size_t deviceIndex, uint16_t startOffset, uint16_t length, uint8_t* buffer) = 0;

private:
    /**
     * @brief Internal implementation for submitting static HSL sequence data.
     *
     * Derived classes must implement this method to handle the actual processing
     * of the static sequence data.
     *
     * @param[in] staticData A ByteView representing the static HSL sequence data.
     * @return HSLResult object indicating the success or failure of the operation.
     */
    virtual HSLResult DoSubmitStaticSequence(ByteView staticData) = 0;
};

} // namespace uddf::cdi

#endif // UDDF_CDI_IHARDWAREACCESS_HPP
