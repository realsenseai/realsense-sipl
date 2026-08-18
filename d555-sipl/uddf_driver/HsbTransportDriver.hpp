/*
 * SPDX-FileCopyrightText: Copyright (c) 2025-2026 RealSense AI. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef UDDF_SAMPLES_HSBTRANSPORTDRIVER_HPP
#define UDDF_SAMPLES_HSBTRANSPORTDRIVER_HPP

#include "HsbTransportDriverBase.hpp"
#include "uddf/cdi/IHardwareAccess.hpp"
#include "uddf/cdi/IDriverServices.hpp"
#include "uddf/ddi/DriverTypeIds.hpp"
#include "uddf/ddi/interfaces/IReadWriteI2C.hpp"
#include "uddf/ddi/interfaces/ICoEBridgeControl.hpp"
#include <string>
#include <string_view>

#include <hololink.hpp>
#include <data_channel.hpp>

#include "HsbExtendedInterface.hpp"

// Forward declare hsb_info for the vector member
struct hsb_info;

namespace uddf::samples {

using namespace uddf::ddi::interfaces;
using namespace uddf::cdi;

/**
 * @brief The declaration of the HsbTransportDriver class.
 *
 * This is the driver for the HSB transport used in Realsense.
 */
class HsbTransportDriver : public HsbTransportDriverBase,
                           public HsbExtendedInterface
{
public:
    /**
     * @brief Construct a new HsbTransportDriver object
     */
    HsbTransportDriver();

    /**
     * @brief Destroy the HsbTransportDriver object
     */
    ~HsbTransportDriver() override;

    /**
     * @brief Gets the unique identifier (UUID) for this driver type.
     * @return the type ID for CoE module drivers..
     */
    uddf::ddi::UUID GetID() const noexcept override {
        return uddf::ddi::drivers::HSB_TRANSPORT_DRIVER_ID;
    }

    // --- ICoEBridgeControl Methods ---

    bool ConfigureDriver(const CoEBridgeContext& context) override;
    bool ProbeHardware(const CoEBridgeContext& context) override;
    bool Init(const CoEBridgeContext& context) override;
    bool SetChannelNumber(const CoEBridgeContext& context, uint32_t channelNumber, size_t cameraIndex) override;
    bool Deinit(const CoEBridgeContext& context) override;
    bool Reset(const CoEBridgeContext& context) override;

    // --- IReadWriteI2C Methods ---

    I2CResult ReadI2C(IDriverServices& driverServices, uint8_t sensorIndex, uint16_t address, uint16_t offset, uint8_t* data, uint16_t length) override;
    I2CResult WriteI2C(IDriverServices& driverServices, uint8_t sensorIndex, uint16_t address, uint16_t offset, uint8_t const* data, uint16_t length) override;

    // --- HsbExtendedInterface Methods ---

    std::vector<hsb_info> EnumerateDevices(uint32_t timeout_seconds, const char *target_interface) override;

protected:
    /**
     * @brief Override to support HsbExtendedInterface
     */
    uddf::ddi::IInterface* GetExtendedInterface(const uddf::ddi::UUID& uuid) noexcept override {
        if (uuid == HsbExtendedInterface::id) {
            return static_cast<HsbExtendedInterface*>(this);
        }
        return nullptr;
    }

private:
    std::shared_ptr<hololink::Hololink> m_hololink;
    std::unique_ptr<hololink::DataChannel> m_dataChan[HSB_MAX_SENSORS];
    std::shared_ptr<hololink::Hololink::I2c> m_i2c[HSB_MAX_SENSORS];
    uint32_t m_hsbId;
    std::vector<hsb_info> m_discovered_devices_transport;

    // Helper function to process enumerated device metadata
    bool processEnumeratedDevice(hololink::Metadata& metadata);

    // Clock configuration
    static std::vector<std::vector<uint8_t>> renesas_bajoran_lite_ts2_clock_config();
};

} // namespace uddf::samples

#endif // UDDF_SAMPLES_HSBTRANSPORTDRIVER_HPP
