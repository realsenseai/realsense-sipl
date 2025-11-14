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

#ifndef UDDF_SAMPLES_SAMPLECOEDRIVER_HPP
#define UDDF_SAMPLES_SAMPLECOEDRIVER_HPP

#include "CoEModuleDriverBase.hpp"
#include "uddf/ddi/interfaces/ISensorControl.hpp"
#include "uddf/ddi/DriverTypeIds.hpp"

#include <string>
#include <string_view>

namespace uddf::samples {

using namespace uddf::ddi::interfaces; // Using namespace directive

/**
 * @brief A sample implementation of the CoEModuleDriver base class.
 *
 * This driver provides a basic, non-functional implementation for demonstration
 * and testing purposes within the UDDF framework. It simulates the presence
 * of a CoE camera module.
 */
class SampleCoEDriver final : public CoEModuleDriverBase,
                              public ISensorControl {
public:
    /**
     * @brief Construct a new Sample CoE Driver object
     * @param[in] variantId An identifier for this specific driver variant (e.g., for logs or configuration).
     */
    explicit SampleCoEDriver(std::string_view variantId);

    /**
     * @brief Destroy the Sample CoE Driver object
     */
    ~SampleCoEDriver() override;

    /**
     * @brief Gets the unique identifier (UUID) for this driver type.
     * @return const uddf::ddi::UUID& The driver type ID.
     */
    uddf::ddi::UUID GetID() const noexcept override {
        return uddf::ddi::drivers::COE_MODULE_DRIVER_ID;
    }

    // --- ICameraModule Methods ---

    /**
     * @brief Retrieves the control interface for a specific sensor (stub implementation).
     * @param[in] context The camera module context.
     * @param[in] index The zero-based index of the sensor.
     * @return ISensorControl* Pointer to the sensor control interface.
     */
    ISensorControl*
    GetSensorControl(const CameraModuleContext& context, size_t index) override;

    /**
     * @brief Starts streaming (stub implementation).
     * @param[in] context The camera module context.
     * @return Always returns true in this sample implementation.
     */
    bool StartStreaming(const CameraModuleContext& context) override;

    /**
     * @brief Stops streaming (stub implementation).
     * @param[in] context The camera module context.
     * @return Always returns true in this sample implementation.
     */
    bool StopStreaming(const CameraModuleContext& context) override;

    // --- ICoEModuleControl Methods ---

    /**
     * @brief Configures the CoE module (stub implementation).
     * @param[in] context The CoE module context.
     * @param[out] deviceTable The I2C device table to populate.
     * @return Always returns true in this sample implementation.
     */
    bool ConfigureDriver(const CoEModuleContext& context, uddf::ddi::DeviceTable& deviceTable) override;

    /**
     * @brief Probes the hardware for the CoE module (stub implementation).
     * @param[in] context The CoE module context.
     * @return Always returns true in this sample implementation.
     */
    bool ProbeHardware(const CoEModuleContext& context) override;

    /**
     * @brief Initializes the CoE module (stub implementation).
     * @param[in] context The CoE module context.
     * @return Always returns true in this sample implementation.
     */
    bool Init(const CoEModuleContext& context) override;

    /**
     * @brief Deinitializes the CoE module (stub implementation).
     * @param[in] context The CoE module context.
     * @return Always returns true in this sample implementation.
     */
    bool Deinit(const CoEModuleContext& context) override;

    /**
     * @brief Resets the CoE module (stub implementation).
     * @param[in] context The CoE module context.
     * @return Always returns true in this sample implementation.
     */
    bool Reset(const CoEModuleContext& context) override;

    // --- ISensorControl Methods ---

    /**
     * @brief Gets the sensor attributes (stub implementation).
     * @param[in] cdi CDI interface reference.
     * @param[out] attributes Output structure for attributes.
     * @return Always returns true in this stub.
     */
    bool GetSensorAttributes(uddf::cdi::IHardwareAccess& hwAccess, SensorAttributes& attributes) const override;

    /**
     * @brief Sets the sensor controls (stub implementation).
     * @param[in] cdi CDI interface reference.
     * @param[in] controls Input structure with desired controls.
     * @return Always returns true in this stub.
     */
    bool SetSensorControls(uddf::cdi::IHardwareAccess& hwAccess, const SensorControls& controls) override;

    /**
     * @brief Parses top embedded data (stub implementation).
     * @param[in] cdi CDI interface reference.
     * @param[in] chunk Input data chunk.
     * @param[out] info Output structure for parsed info.
     * @return Always returns true in this stub.
     */
    bool ParseTopEmbeddedData(uddf::cdi::IHardwareAccess& hwAccess, const EmbeddedDataChunk& chunk, EmbeddedDataInfo& info) override;

    /**
     * @brief Parses bottom embedded data (stub implementation).
     * @param[in] cdi CDI interface reference.
     * @param[in] chunk Input data chunk.
     * @param[out] info Output structure for parsed info.
     * @return Always returns true in this stub.
     */
    bool ParseBottomEmbeddedData(uddf::cdi::IHardwareAccess& hwAccess, const EmbeddedDataChunk& chunk, EmbeddedDataInfo& info) override;

private:
    // This demonstrates a variant of the driver, where the constructor takes a variant ID
    // This is useful for creating multiple instances of the same driver with different configurations
    std::string m_variantId;
};

} // namespace uddf::samples

#endif // UDDF_SAMPLES_SAMPLECOEDRIVER_HPP
