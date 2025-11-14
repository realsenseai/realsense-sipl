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

#ifndef UDDF_DDI_INTERFACES_ICOEMODULECONTROL_HPP
#define UDDF_DDI_INTERFACES_ICOEMODULECONTROL_HPP

#include "uddf/ddi/DDITypes.hpp"
#include "uddf/ddi/IInterface.hpp"
#include "uddf/ddi/uuid.hpp"

#include <string>

namespace uddf::cdi {

class IDriverServices;
class IHardwareAccess;

} // namespace uddf::cdi

namespace uddf::ddi::interfaces {

/**
 * @brief Context required for CoE module control operations.
 */
struct CoEModuleContext {
    /** Non-owning reference to the HardwareAccess implementation. */
    uddf::cdi::IHardwareAccess* hwAccess {nullptr};

    /** Non-owning reference to the DriverServices implementation. */
    uddf::cdi::IDriverServices* driverServices {nullptr};

    /**
     * Configuration data for a CoE module.
     */
    struct Config {
        uint32_t ip {};
        uint8_t  mac[6] {};
        uint16_t width {};
        uint16_t height {};
        float frameRate {};
        PixPackingType pixelFormat {};
    } config;
};

namespace uuid {

inline constexpr UUID COE_CONTROL_INTERFACE_ID = UUID(0xd16d103d, 0x22f8, 0x467a, 0xab07, 0x58, 0x52, 0xf7, 0xb2, 0x89, 0x2d);

} // namespace uuid

/**
 * @brief Interface for Camera over Ethernet (CoE) control functionality.
 *
 * Provides basic lifecycle operations for CoE camera modules.
 */
class ICoEModuleControl : public IInterface {
public:
    /**
     * @brief Static UUID representing the ICoEModuleControl interface type.
     */
    static constexpr UUID id { uuid::COE_CONTROL_INTERFACE_ID };

    /*
     * There are notable omissions in this interface:
     * - Power state notifications
     * - Out-of-band notification mechanism
     */

    /**
     * @brief Configure the driver.
     * The primary purpose of this method is to allow the driver to reject the
     * configuration if it is not valid for whatever reason.  The driver cannot
     * access hardware from this method.  It can, however, set up internal state
     * as needed (although note that the same CoEModuleContext will be passed to
     * all other entrypoints, so copying fields from there is not necessary).
     * The driver should also fill out the I2C device table with the I2C devices
     * that it uses.  The table is used for dynamic HSL programming and for raw
     * I2C reads.
     *
     * This will be the first method called on this interface.
     *
     * @param[in] context The context for the CoE module.
     * Note that the @c hwAccess member will be nullptr for this call.
     * @param[out] deviceTable The I2C device table for the driver.
     * @return true if the configuration was successful, false otherwise.
     */
    virtual bool ConfigureDriver(const CoEModuleContext& context, uddf::ddi::DeviceTable& deviceTable) = 0;

    /**
     * @brief Probe the camera hardware.
     * This method gives the driver an opportunity to probe the hardware,
     * both to ensure that it is supported by this driver and to (perhaps)
     * set up internal state based on the specific hardware type and version.
     * The driver should NOT initialize hardware beyond what's needed to
     * perform the probe; that's what the Init method is for.
     *
     * This will be called after the ConfigureDriver method.
     *
     * @param[in] context The context for the CoE module.
     * @return true if the probe was successful, false otherwise.
     */
    virtual bool ProbeHardware(const CoEModuleContext& context) = 0;

    /**
     * @brief Initialize the camera.
     *
     * This will be called after the ProbeHardware method.
     *
     * @param[in] context The context for the CoE module.
     * @return true if the initialization was successful, false otherwise.
     */
    virtual bool Init(const CoEModuleContext& context) = 0;

    /**
     * @brief Deinitialize the camera.
     *
     * @param[in] context The context for the CoE module.
     * @return true if the deinitialization was successful, false otherwise.
     */
    virtual bool Deinit(const CoEModuleContext& context) = 0;

    /**
     * @brief Reset the camera.
     * This method should reset the camera to a known good state,
     * equivalent to its state after a successful Init().  It will be
     * called to attempt to recover from a failure after Init()
     * originally succeeded.
     *
     * @param[in] context The context for the CoE module.
     * @return true if the reset was successful, false otherwise.
     */
    virtual bool Reset(const CoEModuleContext& context) = 0;

protected:

    ~ICoEModuleControl() override = default;
};

} // namespace uddf::ddi::interfaces

#endif // UDDF_DDI_INTERFACES_ICOEMODULECONTROL_HPP
