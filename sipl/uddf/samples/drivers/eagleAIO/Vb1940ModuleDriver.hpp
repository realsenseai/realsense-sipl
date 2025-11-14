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

#ifndef UDDF_SAMPLES_VB1940MODULEDRIVER_HPP
#define UDDF_SAMPLES_VB1940MODULEDRIVER_HPP

#include "CoEModuleDriverBase.hpp"
#include "uddf/cdi/IHardwareAccess.hpp"
#include "uddf/ddi/interfaces/ISensorControl.hpp"
#include "uddf/ddi/DriverTypeIds.hpp"
#include "Vb1940SensorDriver.hpp"

#include <string>
#include <string_view>

namespace uddf::samples {

using namespace uddf::ddi::interfaces; // Using namespace directive

/**
 * @brief The declaration of the Vb1940ModuleDriver class.
 *
 * This is the driver for the VB1940 camera module used in Eagle.
 */
class Vb1940ModuleDriver final : public CoEModuleDriverBase {
public:
    /**
     * @brief Construct a new Vb1940ModuleDriver object
     */
    explicit Vb1940ModuleDriver();

    /**
     * @brief Destroy the Vb1940ModuleDriver object
     */
    ~Vb1940ModuleDriver() override;

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


protected:

    Vb1940SensorDriver m_sensorDriver;

private:

    /**
     * @brief Log the current state of the camera module
     * @param label Descriptive label for the log entry
     * @param hwAccess Hardware access interface for reading camera state
     */
    void LogState(std::string_view label, uddf::cdi::IHardwareAccess& hwAccess);

    // is the camera currently streaming
    bool m_is_streaming;

};

} // namespace uddf::samples

#endif // UDDF_SAMPLES_VB1940MODULEDRIVER_HPP
