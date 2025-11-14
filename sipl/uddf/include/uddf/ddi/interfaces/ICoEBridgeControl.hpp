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

#ifndef UDDF_DDI_INTERFACES_ICOEBRIDGECONTROL_HPP
#define UDDF_DDI_INTERFACES_ICOEBRIDGECONTROL_HPP

#include "uddf/ddi/DDITypes.hpp"
#include "uddf/ddi/IInterface.hpp"
#include "uddf/ddi/uuid.hpp"

#include <string>

namespace uddf::cdi {

class IDriverServices;

} // namespace uddf::cdi

namespace uddf::ddi::interfaces {

static constexpr uint32_t HSB_MAX_SENSORS { 2U };

/**
 * @brief Context required for CoE bridge (HSB) control operations.
 */
struct CoEBridgeContext {
    /** Non-owning reference to the DriverServices implementation. */
    uddf::cdi::IDriverServices* driverServices {nullptr};

    /**
     * Basic CoE bridge configuration.
     * This information will be available to every entrypoint in the driver.
     * */
    struct BasicConfig {
        std::string interfaceName {};
        uint32_t    hsbId         {0};
        uint32_t    hsbIp         {0};
        bool        vlanEnabled   {false};
    } basicConfig;

    /**
     * Detailed configuration for CoE bridge initialization.
     * This information will be available to the Init entrypoint (and every
     * entrypoint called after Init).
     * */
    struct InitConfig {
        static constexpr uint32_t UDDF_CSI_BRICK_NUM_LANES { 4U };

        // BrickType
        enum BrickType {
            BRICK_DPHY = 0,
            BRICK_CPHY = 1
        };

        struct MipiSettings {
            uint8_t lanes;
            bool lpBypassMode;
            uint8_t tHsSettle;
            uint8_t tClkSettle;
            uint32_t mipiClockRate;
            BrickType brickType;
            uint8_t lanePolarity[UDDF_CSI_BRICK_NUM_LANES];
        };

        struct Resolution {
            uint16_t pitch  {0};
            uint16_t height {0};
        };

        MipiSettings mipiSettings;
        uint32_t ipAddress[HSB_MAX_SENSORS] {0};
        Resolution resolution;
        Resolution embDataFrontResolution;
        Resolution embDataRearResolution;
        PixPackingType pixelFormat {PIX_PACK_RAW10};
        uint8_t channelNumber[HSB_MAX_SENSORS] {0};

        /**
         * Bitmask of sensors that are active on this HSB.
         */
        uint32_t activeSensors {0};

        /**
         * Bitmask of active sensors that should be synchronized.
         */
        uint32_t syncSensors {0};
    } initConfig;
};

namespace uuid {

inline constexpr UUID COE_BRIDGE_CONTROL_INTERFACE_ID = UUID(0xe3dd45f5, 0xae72, 0x4f7f, 0xa996, 0xab, 0x5d, 0xa5, 0x9c, 0x79, 0x24);

} // namespace uuid

/**
 * @brief Interface for Camera over Ethernet (CoE) bridge control functionality.
 *
 * Provides basic lifecycle operations for CoE bridge devices.
 */
class ICoEBridgeControl : public IInterface {
public:
    /**
     * @brief Static UUID representing the ICoEBridgeControl interface type.
     */
    static constexpr UUID id { uuid::COE_BRIDGE_CONTROL_INTERFACE_ID };

    /**
     * @brief Configure the driver.
     * The primary purpose of this method is to allow the driver to reject the
     * configuration if it is not valid for whatever reason.  The driver cannot
     * access hardware from this method.  It can, however, set up internal state
     * as needed (although note that the same CoEBridgeContext will be passed to
     * all other entrypoints, so copying fields from there is not necessary).
     *
     * This will be the first method called on this interface.
     *
     * @param[in] context The context for the CoE bridge.
     * @return true if the configuration was successful, false otherwise.
     */
    virtual bool ConfigureDriver(const CoEBridgeContext& context) = 0;

    /**
     * @brief Probe the bridge hardware.
     * This method gives the driver an opportunity to probe the hardware,
     * both to ensure that it is supported by this driver and to (perhaps)
     * set up internal state based on the specific hardware type and version.
     * The driver should NOT initialize hardware beyond what's needed to
     * perform the probe; that's what the Init method is for.
     *
     * This will be called after the ConfigureDriver method.
     *
     * @param[in] context The context for the CoE bridge.
     * @return true if the probe was successful, false otherwise.
     */
    virtual bool ProbeHardware(const CoEBridgeContext& context) = 0;

    /**
     * Initialize the CoE bridge.
     *
     * This will be called after the ProbeHardware method.
     *
     * @param[in] context The context for the CoE bridge.
     * @return true if the initialization was successful, false otherwise.
     */
    virtual bool Init(const CoEBridgeContext& context) = 0;

    /**
     * Set the channel number for one of the sensors on this bridge.
     */
    virtual bool SetChannelNumber(const CoEBridgeContext& context, uint32_t channelNumber, size_t cameraIndex) = 0;

    /**
     * Deinitialize the CoE bridge.
     *
     * @param[in] context The context for the CoE bridge.
     * @return true if the deinitialization was successful, false otherwise.
     */
    virtual bool Deinit(const CoEBridgeContext& context) = 0;

    /**
     * @brief Reset the bridge.
     * This method should reset the bridge to a known good state,
     * equivalent to its state after a successful Init().  It will be
     * called to attempt to recover from a failure after Init()
     * originally succeeded.
     *
     * @param[in] context The context for the CoE bridge.
     * @return true if the reset was successful, false otherwise.
     */
    virtual bool Reset(const CoEBridgeContext& context) = 0;

protected:

    ~ICoEBridgeControl() override = default;
};

} // namespace uddf::ddi::interfaces

#endif // UDDF_DDI_INTERFACES_ICOEBRIDGECONTROL_HPP
