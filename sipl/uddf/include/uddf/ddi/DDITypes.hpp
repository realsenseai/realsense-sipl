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

#ifndef UDDF_DDI_DDI_TYPES_HPP
#define UDDF_DDI_DDI_TYPES_HPP

#include <cstdint>
#include <vector>

namespace uddf::ddi {

/**
 * A single entry in the I2C device table.
 * The device table lists every I2C device referenced by a driver.
 */
struct DeviceTableEntry
{
    uint8_t i2cAddress;         ///< Physical I2C address of this device
    uint8_t offsetWidth;        ///< Number of bytes in offset for this device
    uint8_t dataWidth;          ///< Number of bytes in data for this device
    uint8_t flags;              ///< Reserved for future device-specific flags (currently unused)
    uint8_t virtualI2cAddress;  ///< Virtual I2C address of this device (GMSL only)
};

using DeviceTable = std::vector<DeviceTableEntry>;

/**
 * Pixel format delivered by a sensor.
 */
enum PixPackingType {
    PIX_PACK_RAW10 = 0,
    PIX_PACK_RAW12 = 1,
    PIX_PACK_RAW16 = 3,
};



} // namespace uddf::ddi

#endif // UDDF_DDI_DDI_TYPES_HPP
