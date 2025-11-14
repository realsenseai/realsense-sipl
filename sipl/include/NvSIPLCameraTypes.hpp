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

#ifndef NVSIPLCAMERATYPES_HPP
#define NVSIPLCAMERATYPES_HPP

#include <variant>
#include <vector>
#include <array>
#include <string>
#include "NvSIPLCommon.hpp"
#include "NvSIPLDeviceBlockInfo.hpp"

namespace nvsipl {

static constexpr size_t MAX_SENSORS_PER_CAMERA = 2;

typedef uint8_t MacAddress[6];

/**
 * Settings for an individual sensor within a camera module.
 */
struct CoESensor {
    /**
     * MAC address of the sensor.
     * Used for identifying the sensor on the Ethernet network.
     */
    MacAddress macAddress;

    /**
     * Optional IP address for this sensor's data path.
     * Only required for non-1722 datapath where HSB uses different IP addresses
     * to communicate with different sensors.
     * Set to 0 if using 1722 datapath or if sensor uses HSB's IP.
     */
    uint32_t ipAddress {0};
};

/**
 * Settings for a camera module connected to an HSB.
 */
struct CoECamera {
    /**
     * Is this being used as a stereo camera?
     * If true, both sensors belong to a single logical camera
     * (and initialization will fail if there is only one sensor).
     * If false, each sensor is treated as an independent camera.
     */
    bool isStereo {false};

    /**
     * Index of the sensor to use if this module has multiple sensors
     * but isStereo is false. Otherwise ignored.
     */
    uint32_t hsbSensorIndex {0};

    /**
     * Configuration for each sensor in this camera module.
     * If @c isStereo is false, even if the module has multiple sensors,
     * only @c sensors[0] will be used.
     */
    CoESensor sensors[MAX_SENSORS_PER_CAMERA];

    /** Id of the HSB that this camera is connected to. */
    uint32_t hsbId;
};

/**
 * Settings for the HSB (Heterogeneous Sensor Bridge).
 */
struct CoETransSettings {
    /** Ethernet interface name on the host. */
    std::string interfaceName;

    /** HSB device name. */
    std::string name;

    /**
     * IP address of the HSB.
     */
    uint32_t ipAddress;

    /**
     * Id of this HSB (must be unique within this configuration).
     */
    uint32_t hsbId;

    /**
     * Flag indicating if VLAN tagging is enabled.
     * Set to true if the network requires VLAN tagging.
     */
    bool vlanEnable {false};

    /**
     * Should multiple sensors on this HSB be synchronized?
     */
    bool syncSensors {true};
};

struct GmslCamera {
    uint32_t linkIndex {UINT32_MAX};
    NvSiplCapInterfaceType csiPort {NVSIPL_CAP_CSI_INTERFACE_TYPE_CSI_A};
    bool isSimulatorModeEnabled {false};
    SerInfo serInfo;
};

struct GmslTransSettings {
    NvSiplCapInterfaceType csiPort {NVSIPL_CAP_CSI_INTERFACE_TYPE_CSI_A};
    DeserInfo deserInfo;
    uint32_t i2cDevice {UINT32_MAX};
    uint32_t desI2CPort {UINT32_MAX};
    uint32_t desTxPort {UINT32_MAX};
    uint32_t pwrPort = UINT32_MAX;
    bool isPassiveModeEnabled {false};
    bool isGroupInitProg {false};
    std::vector<uint32_t> gpios {};
#if !NV_IS_SAFETY
    bool isPwrCtrlDisabled {false};
    bool longCables = {false};
#endif
    bool resetAll;
    std::string bootAcceleratorTag = "";
    bool bootAcceleratorUseStaggeredStart = {false};
};

/**
 * Enum for MIPI lane polarity options
 */
enum class NvSiplMipiPolarity {
    POLARITY_ABC = 0,  ///< Default polarity configuration (A-B-C for CPHY, no swap for DPHY)
    POLARITY_ACB = 1,  ///< A-C-B polarity configuration for CPHY, swap polarity for DPHY
    POLARITY_BCA = 2,  ///< B-C-A polarity configuration for CPHY
    POLARITY_BAC = 3,  ///< B-A-C polarity configuration for CPHY
    POLARITY_CAB = 4,  ///< C-A-B polarity configuration for CPHY
    POLARITY_CBA = 5,  ///< C-B-A polarity configuration for CPHY
    POLARITY_MAX       ///< Max enum value (used for bounds checking)
};

/**
 * Enum for MIPI lane swizzle patterns
 */
enum class NvSiplMipiSwizzle {
    LANE_SWIZZLE_A0A1B0B1 = 0,  ///< Lane swizzle pattern A0A1B0B1 (default)
    LANE_SWIZZLE_A0A1B1B0 = 1,  ///< Lane swizzle pattern A0A1B1B0
    LANE_SWIZZLE_A0B0B1A1 = 2,  ///< Lane swizzle pattern A0B0B1A1
    LANE_SWIZZLE_A0B0A1B1 = 3,  ///< Lane swizzle pattern A0B0A1B1
    LANE_SWIZZLE_A0B1A1B0 = 4,  ///< Lane swizzle pattern A0B1A1B0
    LANE_SWIZZLE_A0B1B0A1 = 5,  ///< Lane swizzle pattern A0B1B0A1
    LANE_SWIZZLE_MAX            ///< Max enum value (used for bounds checking)
};

/**
 * MIPI configuration parameters for camera connections
 */
struct MipiSettings {
    /**
     * Number of data lanes used.
     * Valid range: [1, 4]
     */
    uint8_t lanes = 4;

    /** LP bypass mode. Valid range: [true, false] */
    bool lpBypassMode = false;

    /** MIPI TCLK-SETTLE timing. Valid range: [0, UINT8_MAX] */
    uint8_t tClkSettle = 0;

    /**
     * MIPI clock rate for D-Phy. Symbol rate for C-Phy.
     * Valid range for DPHY in kbps: [40000, 1250000]
     * Valid range for CPHY in ksps: [80000, 3000000]
     */
    uint32_t mipiClockRate = 0;

    /**
     * THS settle time of the MIPI lane, in nanoseconds.
     * If set to 0, the driver attempts to auto-calibrate according to the mclk_multiplier parameter.
     * Calculate the range with:
     * 85 ns + 6×ui < (tHsSettle+6) × (lp_clock_period) < 145 ns + 10×ui
     * Where:
     * - lp_clock_period is 1/(204 MHz)
     * - ui is the unit interval (duration of HS state on clock lane)
     */
    uint8_t tHsSettle = 0;

    /**
     * Lane polarity configuration for each lane/trio.
     *
     * Index corresponds to lane/trio number (0-3):
     * - lanePolarity[0]: Lane/Trio 0 polarity
     * - lanePolarity[1]: Lane/Trio 1 polarity
     * - lanePolarity[2]: Lane/Trio 2 polarity
     * - lanePolarity[3]: Lane/Trio 3 polarity
     *
     * For DPHY: Values are 0 (no swap) or 1 (swap)
     * For CPHY: Each trio can have a polarity configuration 0-5:
     *   - 0: A-B-C (default configuration)
     *   - 1: A-C-B
     *   - 2: B-C-A
     *   - 3: B-A-C
     *   - 4: C-A-B
     *   - 5: C-B-A
     *
     * See NvSiplMipiPolarity enum for reference values.
     */
    std::array<uint8_t, 4> lanePolarity = {{
        static_cast<uint8_t>(NvSiplMipiPolarity::POLARITY_ABC),
        static_cast<uint8_t>(NvSiplMipiPolarity::POLARITY_ABC),
        static_cast<uint8_t>(NvSiplMipiPolarity::POLARITY_ABC),
        static_cast<uint8_t>(NvSiplMipiPolarity::POLARITY_ABC)
    }};

    /**
     * Lane swizzle pattern
     *
     * Controls the ordering/remapping of physical MIPI lanes.
     * Used to accommodate different PCB routing constraints and hardware designs.
     *
     * The pattern defines how logical lanes (A0,A1,B0,B1) are mapped to
     * physical lanes. For example:
     * - LANE_SWIZZLE_A0A1B0B1 (0): Standard ordering (A0→A0, A1→A1, B0→B0, B1→B1)
     * - LANE_SWIZZLE_A0B0A1B1 (3): Alternative (A0→A0, A1→B0, B0→A1, B1→B1)
     *
     * The swizzle pattern applies to all lanes as a group and depends on the number
     * of active lanes specified in the 'lanes' field.
     *
     * @note For 2-lane configurations, only the first two positions in the swizzle
     * pattern are used. For 4-lane configurations, all positions are relevant.
     */
    NvSiplMipiSwizzle laneSwizzle = NvSiplMipiSwizzle::LANE_SWIZZLE_A0A1B0B1;

    /**
     * D-PHY clock rate in kbps
     * Valid range for DPHY in kbps: [40000, 1250000]
     */
    uint32_t dphyRate = {0U};

    /**
     * C-PHY symbol rate in ksps
     * Valid range for CPHY in ksps: [80000, 3000000]
     */
    uint32_t cphyRate = {0U};

    /**
     * PHY mode (D-PHY or C-PHY)
     */
    NvSiplCapCsiPhyMode phyMode = NVSIPL_CAP_CSI_DPHY_MODE;
};

using CameraType = std::variant<CoECamera, GmslCamera>;
using TransportConfig = std::variant<CoETransSettings, GmslTransSettings>;

struct CameraConfig {
    std::string name = "";
    std::string platform = "";
    std::string platformConfig = "";
    std::string description = "";
    bool isEEPROMSupported {false};
    EEPROMInfo eepromInfo;
    SensorInfo sensorInfo;
    std::vector<CryptoKeyInfo> cryptoKeysList {};
    CameraType cameratype;
    MipiSettings mipiSettings;
};

/**
 * Combined structure containing both camera configurations and transport configurations
 * This replaces the separate vectors of CameraConfig and TransportConfig
 */
struct CameraSystemConfig {
    std::vector<CameraConfig> cameras;     // Camera configurations
    std::vector<TransportConfig> transports; // Transport configurations
};

} // namespace nvsipl
#endif // NVSIPLCAMERATYPES_HPP
