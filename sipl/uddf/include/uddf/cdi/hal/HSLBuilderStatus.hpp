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

#ifndef UDDF_CDI_HAL_HSLBUILDERSTATUS_HPP
#define UDDF_CDI_HAL_HSLBUILDERSTATUS_HPP

namespace uddf::cdi {

/**
 * @brief Represents the status of HSL Builder operations based on encoder interaction.
 */
enum class HSLBuilderStatus {
    /** Operation was successful or sequence is valid. */
    OK = 0,
    /** Invalid parameters were provided to an encoder operation. */
    BAD_PARAMETER,
    /** The encoder ran out of space to store the command. */
    OUT_OF_SPACE,
    /** The requested operation is not supported by the encoder. */
    NOT_SUPPORTED,
    /** A generic or unexpected failure occurred in the HSL encoder. */
    ENCODER_FAILURE
};

} // namespace uddf::cdi

#endif // UDDF_CDI_HAL_HSLBUILDERSTATUS_HPP
