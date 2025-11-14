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

#ifndef UDDF_DDI_INTERFACES_ISENSORCONTROL_HPP
#define UDDF_DDI_INTERFACES_ISENSORCONTROL_HPP

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "uddf/ddi/IInterface.hpp"
#include "uddf/ddi/uuid.hpp"

#include "uddf/ddi/interfaces/SensorControlTypes.hpp"

namespace uddf::cdi {

class IHardwareAccess;

} // namespace uddf::cdi

namespace uddf::ddi::interfaces {

/**
 * @brief Namespace for Sensor Control Interface specific UUIDs.
 */
namespace uuid {

/**
 * @brief The unique identifier for the @ref ISensorControl interface type.
 */
inline constexpr UUID SENSOR_CONTROL_INTERFACE_ID = UUID( 0xd5eb7a14, 0x5f5e, 0x8c2b, 0x0000, 0xdf, 0x9e, 0x6b, 0x1d, 0x3c, 0x5a );

} // namespace uuid

using uddf::cdi::IHardwareAccess;

/**
 * @brief Interface for controlling sensor-specific parameters and parsing embedded data.
 *
 * This interface provides methods to query sensor capabilities (attributes) and
 * configure runtime settings (controls) like exposure time, gain, and white balance.
 * It also includes functions to parse sensor-specific metadata embedded within
 * the image data stream (e.g., top or bottom lines).
 */
class ISensorControl : public IInterface {
public:
    /**
     * @brief Static UUID representing the ISensorControl interface type.
     */
    static constexpr UUID id { uuid::SENSOR_CONTROL_INTERFACE_ID };

    /** @brief Alias for the sensor's static attributes structure. */
    using SensorAttributes    = sensor_control::SensorAttributes;
    /** @brief Alias for the sensor's runtime control settings structure. */
    using SensorControls      = sensor_control::SensorControls;
    /** @brief Alias for a chunk of raw embedded data. */
    using EmbeddedDataChunk   = sensor_control::EmbeddedDataChunk;
    /** @brief Alias for the parsed information extracted from embedded data. */
    using EmbeddedDataInfo    = sensor_control::EmbeddedDataInfo;

    /**
     * @brief Retrieves the static attributes of the sensor.
     *
     * @param[in] cdi Reference to the CDI interface.
     * @param[out] attributes Reference to a @ref SensorAttributes structure to be filled
     *                        with the sensor's capabilities.
     * @return `true` if the attributes were successfully retrieved, `false` otherwise.
     */
    virtual bool GetSensorAttributes(IHardwareAccess& hwAccess, SensorAttributes& attributes) const = 0;

    /**
     * @brief Sets runtime control parameters for the sensor.
     *
     * @param[in] cdi Reference to the CDI interface.
     * @param[in] controls A const reference to a @ref SensorControls structure containing
     *                     the desired settings.
     * @return `true` if the controls were successfully applied, `false` otherwise.
     */
    virtual bool SetSensorControls(IHardwareAccess& hwAccess, const SensorControls& controls) = 0;

    /**
     * @brief Parses embedded data found at the top of an image frame.
     *
     * @param[in] cdi Reference to the CDI interface.
     * @param[in] chunk The @ref EmbeddedDataChunk containing the raw data line(s).
     * @param[out] info Reference to an @ref EmbeddedDataInfo structure to be filled
     *                  with the parsed information.
     * @return `true` if parsing was successful, `false` otherwise (e.g., invalid data,
     *         unsupported format).
     */
    virtual bool ParseTopEmbeddedData(IHardwareAccess& hwAccess, const EmbeddedDataChunk& chunk, EmbeddedDataInfo& info) = 0;

    /**
     * @brief Parses embedded data found at the bottom of an image frame.
     *
     * @param[in] cdi Reference to the CDI interface.
     * @param[in] chunk The @ref EmbeddedDataChunk containing the raw data line(s).
     * @param[out] info Reference to an @ref EmbeddedDataInfo structure to be filled
     *                  with the parsed information.
     * @return `true` if parsing was successful, `false` otherwise (e.g., invalid data,
     *         unsupported format).
     */
    virtual bool ParseBottomEmbeddedData(IHardwareAccess& hwAccess, const EmbeddedDataChunk& chunk, EmbeddedDataInfo& info) = 0;

protected:
    /**
     * @brief Protected virtual destructor.
     */
    ~ISensorControl() override = default;
};

} // namespace uddf::ddi::interfaces

#endif // UDDF_DDI_INTERFACES_ISENSORCONTROL_HPP
