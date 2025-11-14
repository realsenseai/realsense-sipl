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

#ifndef UDDF_DRIVER_TYPE_IDS_HPP
#define UDDF_DRIVER_TYPE_IDS_HPP

#include "uddf/ddi/uuid.hpp"

/**
 * @file uddf/ddi/DriverTypeIds.hpp
 *
 * This file contains definitions for the driver type IDs.
 * Each driver type ID signifies a set of interfaces that a driver must implement.
 */

namespace uddf::ddi::drivers {

/**
 * Type ID for a CoE camera module driver.
 * Such drivers must implement the following interfaces:
 * - ICameraModule
 * - ICoEModuleControl
 */
inline constexpr UUID COE_MODULE_DRIVER_ID = UUID(0x54dca323, 0xa5e2, 0x48c2, 0xab49, 0x3a, 0xe2, 0x9b, 0x69, 0x7d, 0xee);

/**
 * Type ID for a HSB transport driver.
 * Such drivers must implement the following interfaces:
 * - ICoEBridgeControl
 * - IReadWriteI2C
 */
inline constexpr UUID HSB_TRANSPORT_DRIVER_ID = UUID(0x1ea9d380, 0x8062, 0x42e7, 0x8840, 0x66, 0x25, 0x81, 0xc0, 0x9a, 0xaa);

} // namespace uddf::ddi::drivers

#endif // UDDF_DRIVER_TYPE_IDS_HPP
