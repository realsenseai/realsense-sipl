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

#ifndef UDDF_SAMPLES_D555MODULEDRIVER_HPP
#define UDDF_SAMPLES_D555MODULEDRIVER_HPP

#include "CoEModuleDriverBase.hpp"
#include "uddf/cdi/IHardwareAccess.hpp"
#include "uddf/ddi/interfaces/ISensorControl.hpp"
#include "uddf/ddi/DriverTypeIds.hpp"
#include "D555SensorDriver.hpp"
#include "D555ExtendedInterface.hpp"

#include <string>
#include <string_view>

namespace uddf::samples {

using namespace uddf::ddi::interfaces; // Using namespace directive

/**
 * @brief The declaration of the D555ModuleDriver class.
 *
 * This is the driver for the D555 camera module used in Realsense.
 */
class D555ModuleDriver final : public CoEModuleDriverBase, D555ExtendedInterface {
public:
    /**
     * @brief Construct a new D555ModuleDriver object
     */
    explicit D555ModuleDriver();

    /**
     * @brief Destroy the D555ModuleDriver object
     */
    ~D555ModuleDriver() override;

    /**
     * @brief Gets the unique identifier (UUID) for this driver type.
     * @return the type ID for CoE module drivers..
     */
    uddf::ddi::UUID GetID() const noexcept override {
        return uddf::ddi::drivers::COE_MODULE_DRIVER_ID;
    }

    // --- ICameraModule Methods ---

    ISensorControl* GetSensorControl(const CameraModuleContext& context, size_t index) override;
    bool StartStreaming(const CameraModuleContext& context) override;
    bool StopStreaming(const CameraModuleContext& context) override;

    // --- ICoEModuleControl Methods ---

    bool ConfigureDriver(const CoEModuleContext& context, uddf::ddi::DeviceTable& deviceTable) override;
    bool ProbeHardware(const CoEModuleContext& context) override;
    bool Init(const CoEModuleContext& context) override;
    bool Deinit(const CoEModuleContext& context) override;
    bool Reset(const CoEModuleContext& context) override;

    bool setStreamId(const uint8_t stream_id) override;

protected:

    /**
     * @brief Override to support D555ExtendedInterface
     */
    uddf::ddi::IInterface* GetExtendedInterface(const uddf::ddi::UUID& uuid) noexcept override {
        if (uuid == D555ExtendedInterface::id) {
            return static_cast<D555ExtendedInterface*>(this);
        }
        return nullptr;
    }

    D555SensorDriver m_sensorDriver;

private:

    // is the camera currently streaming
    bool m_is_streaming;
    uddf::ddi::interfaces::CoEModuleContext::Config m_config{}; // stored copy
};

} // namespace uddf::samples

#endif // UDDF_SAMPLES_D555MODULEDRIVER_HPP
