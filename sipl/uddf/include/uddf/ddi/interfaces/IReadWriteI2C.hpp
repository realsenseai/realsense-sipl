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

#ifndef UDDF_DDI_INTERFACES_IREADWRITEI2C_HPP
#define UDDF_DDI_INTERFACES_IREADWRITEI2C_HPP

#include <cstdint>

#include "uddf/ddi/IInterface.hpp"
#include "uddf/ddi/uuid.hpp"
#include "uddf/cdi/IDriverServices.hpp"

namespace uddf::cdi {

class IHardwareAccess;

} // namespace uddf::cdi

namespace uddf::ddi::interfaces {

namespace uuid {

/**
 * @brief The unique identifier for the @ref IReadWriteI2C interface type.
 */
inline constexpr UUID IREADWRITEI2C_INTERFACE_ID = UUID( 0xacc5ff36, 0x22a0, 0x4d8a, 0x8271, 0xab, 0x97, 0x5b, 0xa8, 0x84, 0xeb );

} // namespace uuid

using uddf::cdi::IHardwareAccess;

/**
 * @brief Interface for reading and writing to I2C devices.
 */
class IReadWriteI2C : public IInterface {
public:
    /**
     * @brief Static UUID representing the IReadWriteI2C interface type.
     */
    static constexpr UUID id { uuid::IREADWRITEI2C_INTERFACE_ID };

    /**
     * @brief Return values from read and write operations.
     */
    enum I2CResult {
        RWI2C_SUCCESS,              ///< Success
        RWI2C_ERROR_NACK,           ///< NACK received
        RWI2C_ERROR_BUS_TIMEOUT,    ///< Bus timeout
        RWI2C_ERROR_ARBLOST,        ///< Arbitration lost
        RWI2C_OUT_OF_RANGE,         ///< Address, offset, or length is out of range
        RWI2C_INTERNAL_ERROR,       ///< Internal error
        RWI2C_ERROR_UNKNOWN,        ///< Unknown error
    };

    /**
     * @brief Read from the I2C bus.
     *
     * @param[in]  sensorIndex The index of the sensor to read from.
     * @param[in]  address The address of the device to read from.
     * @param[in]  offset The starting offset to read from.
     * @param[out] data The data read from the I2C bus.
     * @param[in]  length The number of bytes to read.
     */
    virtual I2CResult ReadI2C(uddf::cdi::IDriverServices& driverServices, uint8_t sensorIndex, uint16_t address, uint16_t offset, uint8_t* data, uint16_t length) = 0;

    /**
     * @brief Write to the I2C bus.
     *
     * @param[in]  sensorIndex The index of the sensor to write to.
     * @param[in]  address The address of the device to write to.
     * @param[in]  offset The starting offset to write to.
     * @param[in]  data The data to write to the I2C bus.
     * @param[in]  length The number of bytes to write.
     */
    virtual I2CResult WriteI2C(uddf::cdi::IDriverServices& driverServices, uint8_t sensorIndex, uint16_t address, uint16_t offset, uint8_t const* data, uint16_t length) = 0;

protected:
    /**
     * @brief Protected virtual destructor.
     */
    ~IReadWriteI2C() override = default;
};

} // namespace uddf::ddi::interfaces

#endif // UDDF_DDI_INTERFACES_IREADWRITEI2C_HPP
