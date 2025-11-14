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

#ifndef UDDF_DDI_INTERFACES_ICAMERAMODULE_HPP
#define UDDF_DDI_INTERFACES_ICAMERAMODULE_HPP

#include "uddf/ddi/IInterface.hpp"
#include "uddf/ddi/interfaces/ISensorControl.hpp"
#include "uddf/ddi/uuid.hpp"

namespace uddf::cdi {

class IDriverServices;
class IHardwareAccess;

} // namespace uddf::cdi

namespace uddf::ddi::interfaces {

namespace uuid {

inline constexpr UUID CAMERA_MODULE_INTERFACE_ID = UUID(0x1b35d6a4, 0xa40, 0x4c21, 0xa0c4, 0x2b, 0x9b, 0x5c, 0x0a, 0x7b, 0x1e);

} // namespace uuid

/**
 * @brief Context required for camera module operations.
 */
struct CameraModuleContext {
    /** Non-owning reference to the CDI implementation. */
    uddf::cdi::IHardwareAccess* hwAccess {nullptr};

    /** Non-owning reference to the DriverServices implementation. */
    uddf::cdi::IDriverServices* driverServices {nullptr};

    /** Additional camera module configuration data can be added here as needed. */
};

/**
 * @brief Interface for controlling a generic camera module.
 *
 * Provides methods to access individual sensors within the module and to start/stop the
 * module's streaming operations.
 *
 * @note The details of the StartStreaming and StopStreaming methods are under development and
 *       will be documented in future revisions.
 */
class ICameraModule : public IInterface {
public:
    /**
     * @brief Static UUID identifying the @ref ICameraModule interface type.
     *
     * @see uddf::ddi::interfaces::uuid::CAMERA_MODULE_INTERFACE_ID
     */
    static constexpr UUID id { uuid::CAMERA_MODULE_INTERFACE_ID };

    /**
     * @brief Retrieves the control interface for a specific sensor within the module.
     *
     * Modules can contain multiple image sensors. This function allows access
     * to the control interface (@ref ISensorControl) for the sensor specified
     * by its zero-based @a index.
     *
     * @param[in] context  The camera module context. See @ref CameraModuleContext.
     *                     Valid value: Must contain a valid CDI handle.
     * @param[in] index    The zero-based index of the sensor whose control
     *                     interface is requested. Valid range: [0..N-1], where N
     *                     is the number of sensors in the module.
     *
     * @retval ISensorControl*  A non-owning pointer to the requested sensor
     *                          control interface if the index is valid.
     * @retval nullptr          If the @a index is out of range or if the sensor
     *                          control interface cannot be retrieved.
     */
    virtual interfaces::ISensorControl* GetSensorControl(const CameraModuleContext& context,
                                                         size_t index) = 0;

    /**
     * @brief Starts the camera module's streaming operations.
     *
     * @param[in] context  The camera module context. See @ref CameraModuleContext.
     *
     * @retval true   The module was started successfully.
     * @retval false  The module could not be started. This could be due to
     *                initialization errors, hardware issues, or communication
     *                problems.
     */
    virtual bool StartStreaming(const CameraModuleContext& context) = 0;

    /**
     * @brief Stops the camera module's streaming operations.
     *
     * @param[in] context  The camera module context. See @ref CameraModuleContext.
     *
     * @retval true   The module was stopped successfully.
     * @retval false  The module could not be stopped cleanly. This might indicate
     *                hardware issues or communication problems during shutdown.
     */
    virtual bool StopStreaming(const CameraModuleContext& context) = 0;

protected:
    /**
     * @brief Protected virtual destructor.
     */
    ~ICameraModule() override = default;
};

} // namespace uddf::ddi::interfaces

#endif // UDDF_DDI_INTERFACES_ICAMERAMODULE_HPP
