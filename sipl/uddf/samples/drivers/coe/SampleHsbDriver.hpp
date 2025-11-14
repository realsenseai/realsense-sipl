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

#ifndef UDDF_SAMPLES_SAMPLEHSBDRIVER_HPP
#define UDDF_SAMPLES_SAMPLEHSBDRIVER_HPP

#include "HsbTransportDriverBase.hpp"
#include "uddf/cdi/IDriverServices.hpp"
#include "uddf/ddi/DriverTypeIds.hpp"
#include "uddf/ddi/interfaces/IReadWriteI2C.hpp"
#include <string>
#include <string_view>

namespace uddf::samples {

using namespace uddf::ddi::interfaces;

/**
 * @brief The declaration of the HsbTransportDriver class.
 *
 * This is the driver for the HSB transport used in Eagle.
 */
class SampleHsbDriver final : public HsbTransportDriverBase {
public:
    /**
     * @brief Construct a new SampleHsbDriver object
     */
    explicit SampleHsbDriver();

    /**
     * @brief Destroy the SampleHsbDriver object
     */
    ~SampleHsbDriver() override;

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

    I2CResult ReadI2C(uddf::cdi::IDriverServices& driverServices, uint8_t sensorIndex, uint16_t address, uint16_t offset, uint8_t* data, uint16_t length) override;
    I2CResult WriteI2C(uddf::cdi::IDriverServices& driverServices, uint8_t sensorIndex, uint16_t address, uint16_t offset, uint8_t const* data, uint16_t length) override;

private:

    // HSB core driver object would be added here in a production implementation
};

} // namespace uddf::samples

#endif // UDDF_SAMPLES_SAMPLEHSBDRIVER_HPP
