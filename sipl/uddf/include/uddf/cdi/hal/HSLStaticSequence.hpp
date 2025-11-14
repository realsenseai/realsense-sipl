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

#ifndef UDDF_CDI_HAL_HSLSTATICSEQUENCE_HPP
#define UDDF_CDI_HAL_HSLSTATICSEQUENCE_HPP

#include <array>
#include <cstdint>

#include "uddf/cdi/ByteView.hpp"

namespace uddf::cdi {

/**
 * @brief A representation of a compiled HSL sequence.
 *
 * @tparam N The size of the byte sequence.
 */
template <size_t N>
class HSLStaticSequence {
public:
    /**
     * @brief Constructs a sequence from a byte array.
     *
     * @param[in] data  The byte array containing the byte data to copy.
     */
    constexpr explicit HSLStaticSequence(const std::array<uint8_t, N>& data) :
        m_blob(data) {}

    /**
     * @brief Provides a non-owning view over the owned HSL sequence.
     *
     * @retval ByteView  A view over the owned sequence.
     */
    ByteView getBlob() const { return m_blob; }

private:
    /** The owned HSL sequence. */
    std::array<uint8_t, N> m_blob;
};

} // namespace uddf::cdi

#endif // UDDF_CDI_HAL_HSLSTATICSEQUENCE_HPP
