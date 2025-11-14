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

#ifndef UDDF_DDI_INTERFACES_IEEPROMACCESS_HPP
#define UDDF_DDI_INTERFACES_IEEPROMACCESS_HPP

#include "uddf/ddi/IInterface.hpp"
#include "uddf/ddi/uuid.hpp"

namespace uddf::cdi {

class IDriverServices;
class IHardwareAccess;

} // namespace uddf::cdi

namespace uddf::ddi::interfaces {

namespace uuid {

inline constexpr UUID EEPROM_ACCESS_INTERFACE_ID = UUID(0x2b318033, 0xfd8e, 0x4280, 0xaaba, 0xbc, 0x09, 0xf7, 0x24, 0xbd, 0x19);

} // namespace uuid

/**
 * @brief Interface for EEPROM access functionality.
 *
 * Provides basic EEPROM operations.
 */
class IEEPromAccess : public IInterface {
public:
    /**
     * @brief Static UUID representing the IEEPromAccess interface.
     */
    static constexpr UUID id { uuid::EEPROM_ACCESS_INTERFACE_ID };

    /**
     * @brief Read data from the EEPROM.
     *
     * @param[in] hwAccess The hardware access interface for the driver to use.
     * @param[in] driverServices The driver services interface for the driver to use.
     * @param[in] address The starting address for the read.
     * @param[out] data The data to read.
     * @param[in] length The number of bytes to read.
     * @return true if the read was successful, false otherwise.
     */
    virtual bool Read(uddf::cdi::IHardwareAccess& hwAccess, uddf::cdi::IDriverServices& driverServices, uint32_t address, uint8_t* data, size_t length) = 0;

protected:

    ~IEEPromAccess() override = default;
};

} // namespace uddf::ddi::interfaces

#endif // UDDF_DDI_INTERFACES_IEEPROMACCESS_HPP
