/*
* SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
* SPDX-License-Identifier: LicenseRef-NvidiaProprietary
*
* NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
* property and proprietary rights in and to this material, related
* documentation and any modifications thereto. Any use, reproduction,
* disclosure or distribution of this material and related documentation
* without an express license agreement from NVIDIA CORPORATION or
* its affiliates is strictly prohibited.
*/

#include <iostream>
#include <chrono>   // multi-camera link0 RGB RCLKOUT re-pulse settle delays

#include "uddf/cdi/IHardwareAccess.hpp"
#include "uddf/ddi/IDriver.hpp"
#include "uddf/ddi/interfaces/IGmslDeserializer.hpp"

#include "MAX967XX.hpp"
#include "MAX967XXHsl.hpp"
#include "uddf/ddi/interfaces/config/ITunnelingMode.hpp"

namespace uddf::cdd {

using namespace uddf::ddi::interfaces;
using namespace uddf::cdi;

// PHY replication routing registers (common to MAX96712 and MAX96724)
constexpr uint16_t REG_PHY_CPY0 = 0x08A9U;  ///< Replication slot 0
constexpr uint16_t REG_PHY_CPY1 = 0x08AAU;  ///< Replication slot 1

// Pipeline mapping: which PHYs have active video (used to validate replication source)
constexpr uint16_t REG_PIPELINE_ENABLE = 0x00F4U;   ///< Pipeline enable per link (bit N = link N)
constexpr uint16_t REG_PIPELINE_DEST_BASE = 0x092DU; ///< Link 0 pipeline-to-PHY mapping (MAP_DPHY_DEST)
constexpr uint16_t REG_PIPELINE_DEST_STRIDE = 0x40U; ///< Stride to Link 1/2/3 mapping regs (0x096D, 0x09AD, 0x09ED)

MAX967XX::MAX967XX(MAXDeserType deserType) : m_deserType(deserType) {
}

MAX967XX::~MAX967XX() {
}

uddf::ddi::IInterface* MAX967XX::GetInterface(const uddf::ddi::UUID& uuid) noexcept {
    if (uuid == IGmslDeserializer::id) {
        return static_cast<IGmslDeserializer*>(this);
    } else if (uuid == IConfigurationObject::id) {
        if (m_configObject) {
            return static_cast<IConfigurationObject*>(m_configObject.get());
        }
    }

    // Interface not supported
    return nullptr;
}

void MAX967XX::SetMipiOutputMode(DeserializerContext& context) {
    // Determine MIPI output mode based on CSI port configuration
    // Combined ports (AB, CD, EF, GH) use 4-lane mode (2x4)
    // Single ports (A, B, C, D, E, F, G, H) use 2-lane mode (4x2)
    switch (context.basicConfig.csiPort) {
        case DeserializerContext::CsiPortType::CSI_PORT_AB:
        case DeserializerContext::CsiPortType::CSI_PORT_CD:
        case DeserializerContext::CsiPortType::CSI_PORT_EF:
        case DeserializerContext::CsiPortType::CSI_PORT_GH:
            // 4-lane combined port - use 2x4 mode (2 ports, 4 lanes each)
            m_config.mipiOutputMode = uddf::cdd::MipiOutputMode::CDI_MAX967XX_MIPI_OUT_2x4;
            UDDF_LOG_DEBUG(*context.driverServices, "MIPI output mode set to 2x4 (4-lane)");
            break;
        case DeserializerContext::CsiPortType::CSI_PORT_A:
        case DeserializerContext::CsiPortType::CSI_PORT_B:
        case DeserializerContext::CsiPortType::CSI_PORT_C:
        case DeserializerContext::CsiPortType::CSI_PORT_D:
        case DeserializerContext::CsiPortType::CSI_PORT_E:
        case DeserializerContext::CsiPortType::CSI_PORT_F:
        case DeserializerContext::CsiPortType::CSI_PORT_G:
        case DeserializerContext::CsiPortType::CSI_PORT_H:
            // 2-lane single port - use 4x2 mode (4 ports, 2 lanes each)
            m_config.mipiOutputMode = uddf::cdd::MipiOutputMode::CDI_MAX967XX_MIPI_OUT_4x2;
            UDDF_LOG_DEBUG(*context.driverServices, "MIPI output mode set to 4x2 (2-lane)");
            break;
        case DeserializerContext::CsiPortType::CSI_PORT_NONE:
        default:
            // Default to none mode
            m_config.mipiOutputMode = uddf::cdd::MipiOutputMode::CDI_MAX967XX_MIPI_OUT_NONE;
            UDDF_LOG_DEBUG(*context.driverServices, "MIPI output mode defaulted to none");
            break;
    }
}

bool MAX967XX::ConfigureDriver(DeserializerContext& context,
    uddf::ddi::DeviceTable& i2cDevices, uddf::ddi::GpioPinTable& gpioPins) {
    UDDF_LOG_DEBUG(*context.driverServices, "ConfigureDriver called");
    // i2cDevices and gpioPins,
    // set 0x27 as default address, if not available from config.
    uint8_t deviceAddress = 0x27;
    if (context.basicConfig.i2cAddress != 0) {
        deviceAddress = context.basicConfig.i2cAddress;
    }
    i2cDevices.push_back(uddf::ddi::DeviceTableEntry{
        .i2cAddress = deviceAddress,
        .offsetWidth = 2,
        .dataWidth = 1,
        .flags = 0x04,
        .hslI2cAddress = max967xx::hsl::MAX967XX_HSL_I2C_ADDRESS,
    });
    // MULTI-CAMERA: allow the deser to reach link0's MAX9295 serializer (0x40, 8-bit data / 16-bit
    // offset) so StartStreaming can re-pulse link0's RGB reference clock (RCLKOUT) after a per-link
    // isolation toggle glitched it. Cross-driver co-declaration of 0x40 (the ser also declares it) is
    // accepted by the HSL encoder (same as the ser declaring the deser 0x29).
    i2cDevices.push_back(uddf::ddi::DeviceTableEntry{
        .i2cAddress = 0x40,
        .offsetWidth = 2,
        .dataWidth = 1,
        .flags = 0,
    });

    // This driver does not use any GPIO pins
    static_cast<void>(gpioPins);

    // Set MIPI output mode based on CSI port
    SetMipiOutputMode(context);

    return true;
}

bool MAX967XX::ProbeHardware(DeserializerContext& context, bool alreadyInitialized) {
    UDDF_LOG_DEBUG(*context.driverServices, "ProbeHardware called");

    // Call the check_presence HSL sequence to verify device is present
    if (!doCheckPresence(context)) {
        return false;
    }

    return true;
}

bool MAX967XX::SetDefault(DeserializerContext& context) {
    UDDF_LOG_DEBUG(*context.driverServices, "SetDefault called");

    uint8_t linkMask = context.initConfig.linkMask;

    // Call the activate_link_reset HSL sequence to configure the device
    max967xx::hsl::LinkResetData linkResetData = {
        .linkResetData = {{ static_cast<uint8_t>(linkMask << 4) }},
    };
    context.hwAccess->SubmitSequence(max967xx::hsl::activate_link_reset, linkResetData);

    // Set GMSL Link Mode and Speed
    // TODO: Add other modes
    for (uint8_t i = 0; i < MAX_DESERIALIZER_LINKS; i++) {
        if (context.initConfig.linkModes[i].linkMode ==
            uddf::ddi::interfaces::DeserializerLinkMode::LINK_MODE_GMSL2_6GBPS) {
            uint8_t const linkIndex = context.initConfig.linkModes[i].linkIndex;
            switch (linkMask & (1 << linkIndex)) {
                case 1:
                    context.hwAccess->SubmitSequence(max967xx::hsl::setup_gmsl2_6gbps_link_a);
                    break;
                case 2:
                    context.hwAccess->SubmitSequence(max967xx::hsl::setup_gmsl2_6gbps_link_b);
                    break;
                case 4:
                    context.hwAccess->SubmitSequence(max967xx::hsl::setup_gmsl2_6gbps_link_c);
                    break;
                case 8:
                    context.hwAccess->SubmitSequence(max967xx::hsl::setup_gmsl2_6gbps_link_d);
                    break;
                default:
                    UDDF_LOG_ERROR(*context.driverServices, "Unsupported link mask: 0x%x for link index: %d", linkMask, linkIndex);
                    break;
            }
        } else if (context.initConfig.linkModes[i].linkMode ==
            uddf::ddi::interfaces::DeserializerLinkMode::LINK_MODE_GMSL2_3GBPS) {
            uint8_t const linkIndex = context.initConfig.linkModes[i].linkIndex;
            switch (linkMask & (1 << linkIndex)) {
                case 1:
                    context.hwAccess->SubmitSequence(max967xx::hsl::setup_gmsl2_3gbps_link_a);
                    break;
                case 2:
                    context.hwAccess->SubmitSequence(max967xx::hsl::setup_gmsl2_3gbps_link_b);
                    break;
                case 4:
                    context.hwAccess->SubmitSequence(max967xx::hsl::setup_gmsl2_3gbps_link_c);
                    break;
                case 8:
                    context.hwAccess->SubmitSequence(max967xx::hsl::setup_gmsl2_3gbps_link_d);
                    break;
                default:
                    UDDF_LOG_ERROR(*context.driverServices, "Unsupported link mask: 0x%x for link index: %d", linkMask, linkIndex);
                    break;
            }
        } else if (context.initConfig.linkModes[i].linkMode ==
                   uddf::ddi::interfaces::DeserializerLinkMode::LINK_MODE_NONE) {
            // Ignore this link (TODO this should go away when we redesign the link mode config)
        } else {
            UDDF_LOG_ERROR(*context.driverServices, "Unsupported link mode");
            return false;
        }
    }

    // Call the set_i2c_port HSL sequence to configure the I2C port
    // TODO: Recheck and update according the links, for now updated for link A
    SetI2cPort(context);

    // Call the set_i2c_mode HSL sequence to configure the I2C mode
    context.hwAccess->SubmitSequence(max967xx::hsl::set_i2c_mode);

    // Call Disable all pipelines
    context.hwAccess->SubmitSequence(max967xx::hsl::disable_all_pipelines);

    // Call the release_link_reset HSL sequence to configure the device
    context.hwAccess->SubmitSequence(max967xx::hsl::release_link_reset);

    return true;
}

namespace {

constexpr uint32_t MAX_PHY_RATE = 2500000U;   // Maximum PHY rate (2.5 Gsps / 2.5 Gbps)
constexpr uint32_t MIPI_SPEED_DIVISOR = 100000U;
constexpr uint8_t MASK_BITS_6_7 = 0xC0U;  // Preserve bits 6-7
constexpr uint8_t BIT_5_SET = 0x20U;       // Bit 5 must be set

} // namespace

void MAX967XX::prepareMipiPhyConfig(DeserializerContext& context,
                                         uint32_t phyRate,
                                         const char* phyName,
                                         max967xx::hsl::MipiPhyDatarateConfig& mipiConfig) {
    uint8_t rateIndex = (m_config.mipiOutputMode == MipiOutputMode::CDI_MAX967XX_MIPI_OUT_4x2) ? 1U : 0U;

    if (phyRate == 0U) {
        phyRate = MAX_PHY_RATE;
        UDDF_LOG_WARNING(*context.driverServices,
                         "No %s datarate provided for rate index %u, setting to max value: %u kHz",
                         phyName, rateIndex, phyRate);
    }

    uint8_t mipiSpeed = static_cast<uint8_t>(phyRate / MIPI_SPEED_DIVISOR);
    if (mipiSpeed < 1U) mipiSpeed = 1U;
    if (mipiSpeed > 25U) mipiSpeed = 25U;

    mipiConfig.mipiSpeed[0] = mipiSpeed;

    UDDF_LOG_DEBUG(*context.driverServices,
                   "Setting %s: rate=%u kHz, mipiSpeed=%u (0x%02X)",
                   phyName, phyRate, mipiSpeed, mipiSpeed);

    context.hwAccess->SubmitSequence(max967xx::hsl::read_mipi_phy_registers, mipiConfig);

    mipiConfig.reg_0415_value[0] = (mipiConfig.reg_0415_current[0] & MASK_BITS_6_7) | (BIT_5_SET | mipiSpeed);
    mipiConfig.reg_0418_value[0] = (mipiConfig.reg_0418_current[0] & MASK_BITS_6_7) | (BIT_5_SET | mipiSpeed);
    mipiConfig.reg_041B_value[0] = (mipiConfig.reg_041B_current[0] & MASK_BITS_6_7) | (BIT_5_SET | mipiSpeed);
    mipiConfig.reg_041E_value[0] = (mipiConfig.reg_041E_current[0] & MASK_BITS_6_7) | (BIT_5_SET | mipiSpeed);
}

void MAX967XX::SetMipiCPhy(DeserializerContext& context) {
    uint8_t rateIndex = (m_config.mipiOutputMode == MipiOutputMode::CDI_MAX967XX_MIPI_OUT_4x2) ? 1U : 0U;
    uint32_t phyRate = context.basicConfig.cphyRate[rateIndex];

    max967xx::hsl::MipiPhyDatarateConfig mipiConfig = {};
    prepareMipiPhyConfig(context, phyRate, "CPHY", mipiConfig);

    context.hwAccess->SubmitSequence(max967xx::hsl::set_mipi_c_phy, mipiConfig);
}

void MAX967XX::SetMipiDPhy(DeserializerContext& context) {
    uint8_t rateIndex = (m_config.mipiOutputMode == MipiOutputMode::CDI_MAX967XX_MIPI_OUT_4x2) ? 1U : 0U;
    uint32_t phyRate = context.basicConfig.dphyRate[rateIndex];

    max967xx::hsl::MipiPhyDatarateConfig mipiConfig = {};
    prepareMipiPhyConfig(context, phyRate, "DPHY", mipiConfig);

    bool is4x2Mode = (m_config.mipiOutputMode == MipiOutputMode::CDI_MAX967XX_MIPI_OUT_4x2);
    doSubmitSetMipiDPhy(context, mipiConfig, is4x2Mode);
}

bool MAX967XX::SetMipiConfig(DeserializerContext& context) {

    auto result = true;

    if (context.basicConfig.phyMode == PhyMode::CPHY) {
        SetMipiCPhy(context);
    } else if (context.basicConfig.phyMode == PhyMode::DPHY) {
        SetMipiDPhy(context);
        // Deskew is required for DPHY mode with MIPI speed >= 1.5GHz (1500000 kHz)
        // Index 0 = 2x4 mode, Index 1 = 4x2 mode
        uint8_t rateIndex = (m_config.mipiOutputMode == MipiOutputMode::CDI_MAX967XX_MIPI_OUT_4x2) ? 1U : 0U;
        if (context.basicConfig.dphyRate[rateIndex] >= 1500000U) {
            context.hwAccess->SubmitSequence(max967xx::hsl::setup_initial_deskew_all_controllers);
        }
    } else {
        UDDF_LOG_ERROR(*context.driverServices, "Unsupported PHY mode");
        return false;
    }

    return result;
}

bool MAX967XX::Init(DeserializerContext& context) {
    UDDF_LOG_DEBUG(*context.driverServices, "Init called");

    // Find out if tunneling mode is enabled for each link
    for (uint8_t i = 0; i < MAX_DESERIALIZER_LINKS; i++) {
        auto tunnelingIf = interface_cast<config::ITunnelingMode>(context.initConfig.moduleConfigObjects[i]);
        if (tunnelingIf) {
            m_tunnelingModes[i] = tunnelingIf->IsTunnelingModeEnabled();
        } else {
            m_tunnelingModes[i] = false;  // (just to be clear about the default)
        }
    }

    // Call SetDefault
    auto result = SetDefault(context);
    if (!result) {
        UDDF_LOG_ERROR(*context.driverServices, "SetDefault sequence failed");
        return false;
    }

    result = SetMipiConfig(context);
    if (!result) {
        UDDF_LOG_ERROR(*context.driverServices, "SetMipiConfig sequence failed");
        return false;
    }

    // Call disable_csi_out
    context.hwAccess->SubmitSequence(max967xx::hsl::disable_csi_out);

    // Call reset_links
    context.hwAccess->SubmitSequence(max967xx::hsl::reset_links);

    return true;
}

bool MAX967XX::InitWithSerializer(DeserializerContext& context, uint8_t linkIndex) {
    UDDF_LOG_DEBUG(*context.driverServices, "InitWithSerializer called");

    if (linkIndex >= MAX_DESERIALIZER_LINKS) {
        UDDF_LOG_ERROR(*context.driverServices, "Invalid link index: 0x%x", linkIndex);
        return false;
    }

    // Convert linkIndex to linkMask
    uint8_t linkMask = 1 << linkIndex;
    auto result = CheckLinkLock(context, linkMask);
    if (!result) {
        UDDF_LOG_ERROR(*context.driverServices, "CheckLinkLock sequence failed");
        return false;
    }

    // Enable GPIO RX (SetSERDESGpioForward -> Enable GPIO RX)
    context.hwAccess->SubmitSequence(max967xx::hsl::set_serdes_gpio_forward);

    // Enable double pixel mode (SetSERPhy -> Enable double pixel mode)
    {
        // TODO: Add support for other data types
        if (context.basicConfig.dataType == ImageFormatDataType::RAW12) {
            context.hwAccess->SubmitSequence(max967xx::hsl::enable_double_pixel_mode);
        } else if (context.basicConfig.dataType == ImageFormatDataType::RAW10) {
            // TODO: Implement double pixel mode for RAW10 data type,
            // possibly needed for Embedded data type-8-bits support.
            UDDF_LOG_INFO(*context.driverServices,
                          "RAW10 data type is not yet supported for double pixel mode.");
        } else {
            UDDF_LOG_DEBUG(*context.driverServices,
                           "Unsupported data type, skipping double pixel mode");
        }

        // Un-double 8-bit embedded data when expecting EMB8 data type to be enabled
        // This sequence uses WriteMasked to avoid clearing other bits.
        if (linkIndex < MAX_DESERIALIZER_LINKS &&
            context.initConfig.isEmbeddedDataTypeEnabled[linkIndex]) {
            context.hwAccess->SubmitSequence(max967xx::hsl::set_undouble_8bit_emb_data);
        }

        // enable_double_pixel_mode_ext_{A,B,C,D} write VIDEO_RX0 for deser pipe==linkIndex
        // (0x0100 + 0x12*link). That pipe-per-link assumption is WRONG for the D457 multi-VC 6-pipe
        // layout, where link0 uses pipes 0/1/2 and link1 uses 3/4/5: e.g. ext_B (link1) writes 0x0112
        // = pipe1's VIDEO_RX = LINK0's RGB pipe, clobbering the DIS_PKT_DET (0x23) that mapping_A set and
        // STALLING link0 RGB. This ext block is only meaningful for double-pixel (RAW12) modes; the D457
        // is RAW16, so skip it entirely and preserve mapping_A's per-pipe VIDEO_RX config. (Root cause of
        // the 2-cam link0-rgb=0; verified on-rig: link1 bring-up alone, no DS5 play, killed link0 rgb.)
        if (context.basicConfig.dataType == ImageFormatDataType::RAW12) {
            switch (linkIndex) {
                case 0:
                    context.hwAccess->SubmitSequence(max967xx::hsl::enable_double_pixel_mode_ext_A);
                    break;
                case 1:
                    context.hwAccess->SubmitSequence(max967xx::hsl::enable_double_pixel_mode_ext_B);
                    break;
                case 2:
                    context.hwAccess->SubmitSequence(max967xx::hsl::enable_double_pixel_mode_ext_C);
                    break;
                case 3:
                    context.hwAccess->SubmitSequence(max967xx::hsl::enable_double_pixel_mode_ext_D);
                    break;
                default:
                    UDDF_LOG_ERROR(*context.driverServices, "Unsupported link index: 0x%x", linkIndex);
                    return false;
            }
        }
    }

    // Configure video pipeline mapping for this link, for single or dual-sensor module
    result = ConfigureVideoPipelineMappingForLink(context, linkIndex);
    if (!result) {
        UDDF_LOG_ERROR(*context.driverServices, "ConfigureVideoPipelineMappingForLink failed for link %u", linkIndex);
        return false;
    }

    // Enable video pipeline
    // Register 0xF4 (VIDEO_PIPE_EN) has one bit per video pipe (bit N = pipe N).
    // Single-sensor: each link uses 1 pipe  → enable 1 bit  (linkIndex)
    // Dual-sensor:   each link uses 2 pipes → enable 2 bits (linkIndex*2) and (linkIndex*2+1)
    //
    // NOTE: Mixed single/dual-sensor links on the same deserializer are NOT supported --
    // The pipe numbering schemes are incompatible: single-sensor uses pipe N for link N;
    // dual-sensor uses pipes 2*N and 2*N+1 for link N.
    uint8_t regData = 0x00U;
    context.hwAccess->ReadI2C(context.basicConfig.i2cAddress, 0x00F4, 1, &regData);

    uint8_t pipeBits;
    if (IsDualSensorConfig(context, linkIndex)) {
        // Enable two consecutive pipes: e.g. Link A → pipes 0,1 → 0x03 << 0
        pipeBits = static_cast<uint8_t>(0x03U << (linkIndex * 2U));
    } else {
        // Enable single pipe: e.g. Link A → pipe 0 → 0x01 << 0
        pipeBits = static_cast<uint8_t>(0x01U << linkIndex);
    }

    max967xx::hsl::LinkIndexData linkIndexData = {
        .linkIndex = {{ static_cast<uint8_t>(regData | pipeBits) }},
    };
    context.hwAccess->SubmitSequence(max967xx::hsl::video_pipeline_enable, linkIndexData);

    // Unset ERRB_RX_EN (Init_Errb -> Unset ERRB_RX_EN)
    switch (linkIndex) {
        case 0:
            context.hwAccess->SubmitSequence(max967xx::hsl::unset_errb_rx_en_A);
            break;
        case 1:
            context.hwAccess->SubmitSequence(max967xx::hsl::unset_errb_rx_en_B);
            break;
        case 2:
            context.hwAccess->SubmitSequence(max967xx::hsl::unset_errb_rx_en_C);
            break;
        case 3:
            context.hwAccess->SubmitSequence(max967xx::hsl::unset_errb_rx_en_D);
            break;
        default:
            break;
    }

    if (context.basicConfig.fsyncMode == FsyncMode::EXTERNAL
        && context.initConfig.linkModes[linkIndex].linkMode !=
            DeserializerLinkMode::LINK_MODE_GMSL1) {
        uint8_t const txId = context.initConfig.fsyncTxIdPerLink[linkIndex];
        UDDF_LOG_INFO(*context.driverServices,
            "Deserializer: configuring EXTERNAL FSYNC for link %u (GPIO MFP2, TX ID 0x%02X)", linkIndex, txId);

        // GPIO TX routing register format (0x0307/033D/0374/03AA):
        //   Bit 7: GPIO_OUT_DIS (0 = output enabled, 1 = output disabled)
        //   Bit 5: GPIO_TX_EN  (1 = transmit enabled)
        //   Bits [4:0]: GPIO_TX_ID
        // Link A uses 0xA0 (bit7=1, bit5=1): configures open-drain output + TX enable.
        // Links B/C/D use 0x20 (bit5=1): TX enable only (output type unchanged).
        max967xx::hsl::FsyncConfig fsyncConfig = {
            .linkA_gpio_tx_id = {{ static_cast<uint8_t>(0xA0U | txId) }},
            .linkB_gpio_tx_id = {{ static_cast<uint8_t>(0x20U | txId) }},
            .linkC_gpio_tx_id = {{ static_cast<uint8_t>(0x20U | txId) }},
            .linkD_gpio_tx_id = {{ static_cast<uint8_t>(0x20U | txId) }},
        };

        switch (linkIndex) {
            case 0:
                context.hwAccess->SubmitSequence(max967xx::hsl::set_gmsl2_external_fsync_link_A, fsyncConfig);
                break;
            case 1:
                context.hwAccess->SubmitSequence(max967xx::hsl::set_gmsl2_external_fsync_link_B, fsyncConfig);
                break;
            case 2:
                context.hwAccess->SubmitSequence(max967xx::hsl::set_gmsl2_external_fsync_link_C, fsyncConfig);
                break;
            case 3:
                context.hwAccess->SubmitSequence(max967xx::hsl::set_gmsl2_external_fsync_link_D, fsyncConfig);
                break;
            default:
                break;
        }
    } else if (context.basicConfig.fsyncMode == FsyncMode::OSC_MANUAL) {
        if (!m_config.internalFsyncConfigured) {
            UDDF_LOG_INFO(*context.driverServices,
                "Deserializer: configuring INTERNAL FSYNC (30Hz generator, TX ID 8) on link %u", linkIndex);
            doSetInternalFsync(context);
            m_config.internalFsyncConfigured = true;
        }
    } else {
        UDDF_LOG_ERROR(*context.driverServices, "Unsupported FSYNC mode");
        return false;
    }

    return true;
}

bool MAX967XX::FinalizeInitWithSerializer(DeserializerContext& context, uint8_t linkIndex) {
    UDDF_LOG_DEBUG(*context.driverServices, "FinalizeInitWithSerializer called");

    // Misc Init -> Set ERRB_RX_EN
    switch (linkIndex) {
        case 0:
            context.hwAccess->SubmitSequence(max967xx::hsl::set_errb_rx_en_A);
            break;
        case 1:
            context.hwAccess->SubmitSequence(max967xx::hsl::set_errb_rx_en_B);
            break;
        case 2:
            context.hwAccess->SubmitSequence(max967xx::hsl::set_errb_rx_en_C);
            break;
        case 3:
            context.hwAccess->SubmitSequence(max967xx::hsl::set_errb_rx_en_D);
            break;
        default:
            break;
    }

    // Post Sensor Init -> Set FSYNC
    // TODO: Need to check why we set multiple times on TOT.
    return true;
}

bool MAX967XX::ControlLink(DeserializerContext& context, uint8_t linkIndex, bool enableLink) {
    UDDF_LOG_DEBUG(*context.driverServices, "ControlLink called");

    if (enableLink) {
        switch (linkIndex) {
            case 0:
                context.hwAccess->SubmitSequence(max967xx::hsl::enable_link_A);
                break;
            case 1:
                context.hwAccess->SubmitSequence(max967xx::hsl::enable_link_B);
                break;
            case 2:
                context.hwAccess->SubmitSequence(max967xx::hsl::enable_link_C);
                break;
            case 3:
                context.hwAccess->SubmitSequence(max967xx::hsl::enable_link_D);
                break;
            default:
                UDDF_LOG_ERROR(*context.driverServices, "Unsupported link index: 0x%x", linkIndex);
                return false;
        }
    } else {
        switch (linkIndex) {
            case 0:
                context.hwAccess->SubmitSequence(max967xx::hsl::disable_link_A);
                break;
            case 1:
                context.hwAccess->SubmitSequence(max967xx::hsl::disable_link_B);
                break;
            case 2:
                context.hwAccess->SubmitSequence(max967xx::hsl::disable_link_C);
                break;
            case 3:
                context.hwAccess->SubmitSequence(max967xx::hsl::disable_link_D);
                break;
            default:
                UDDF_LOG_ERROR(*context.driverServices, "Unsupported link index: 0x%x", linkIndex);
                return false;
        }
    }

    return true;
}

bool MAX967XX::CheckLinkLock(DeserializerContext& context, uint8_t linkMask) {
    UDDF_LOG_DEBUG(*context.driverServices, "CheckLinkLock called");

    for (uint8_t i = 0; i < MAX_DESERIALIZER_LINKS; i++) {
        switch (linkMask & (1 << i)) {
            case 1:
                context.hwAccess->SubmitSequence(max967xx::hsl::check_link_lock_A);
                break;
            case 2:
                context.hwAccess->SubmitSequence(max967xx::hsl::check_link_lock_B);
                break;
            case 4:
                context.hwAccess->SubmitSequence(max967xx::hsl::check_link_lock_C);
                break;
            case 8:
                context.hwAccess->SubmitSequence(max967xx::hsl::check_link_lock_D);
                break;
            default:
                break;
        }
    }

    // TODO: Check if link lock time out then run One Shot Reset and check link lock again

    return true;
}

bool MAX967XX::ConfigurePhyReplication(DeserializerContext& context) {
    auto const& [slot0, slot1] = context.basicConfig.replicationInfo;

    if (!slot0.has_value() && !slot1.has_value()) {
        return true;
    }

    uint8_t pipelineEnable = 0U;
    if (!context.hwAccess->ReadI2C(context.basicConfig.i2cAddress,
                                    REG_PIPELINE_ENABLE, 1, &pipelineEnable)) {
        UDDF_LOG_ERROR(*context.driverServices,
                       "Replication: failed to read pipeline enable (0x%04X)",
                       REG_PIPELINE_ENABLE);
        return false;
    }

    // For each enabled link, read pipeline mapping reg bits [1:0] (value 0–3 = destination PHY).
    // Replication srcPhy must match one of these values.
    bool validSrcPhy[4] = {false};
    for (uint32_t i = 0U; i < 4U; ++i) {
        if ((pipelineEnable & (1U << i)) == 0U) {
            continue;
        }
        uint16_t const reg = REG_PIPELINE_DEST_BASE + i * REG_PIPELINE_DEST_STRIDE;
        uint8_t regVal = 0U;
        if (!context.hwAccess->ReadI2C(context.basicConfig.i2cAddress, reg, 1, &regVal)) {
            UDDF_LOG_ERROR(*context.driverServices,
                           "Replication: failed to read pipeline mapping (0x%04X)", reg);
            return false;
        }
        uint8_t const phyFromReg = regVal & 3U;  // 0, 1, 2, or 3
        validSrcPhy[phyFromReg] = true;
    }

    auto validate = [&](auto const& entry) -> bool {
        if (entry.srcPhy == entry.dstPhy) {
            UDDF_LOG_ERROR(*context.driverServices,
                           "Replication: srcPhy (%u) == dstPhy (%u)",
                           entry.srcPhy, entry.dstPhy);
            return false;
        }
        if (entry.srcPhy > 3U || entry.dstPhy > 3U) {
            UDDF_LOG_ERROR(*context.driverServices,
                           "Replication: PHY index out of range (src=%u, dst=%u, max=3)",
                           entry.srcPhy, entry.dstPhy);
            return false;
        }
        // srcPhy must equal one of the pipeline mapping values (0–3) read above
        if (!validSrcPhy[entry.srcPhy]) {
            UDDF_LOG_ERROR(*context.driverServices,
                           "Replication: srcPhy (%u) does not match any pipeline mapping "
                           "(0x092D/0x096D/0x09AD/0x09ED bits [1:0])",
                           entry.srcPhy);
            return false;
        }
        return true;
    };

    if (slot0.has_value() && !validate(slot0.value())) {
        return false;
    }
    if (slot1.has_value() && !validate(slot1.value())) {
        return false;
    }

    if (slot0.has_value() && slot1.has_value() &&
        slot0->srcPhy == slot1->srcPhy && slot0->dstPhy == slot1->dstPhy) {
        UDDF_LOG_ERROR(*context.driverServices,
                       "Replication: duplicate entry (src=%u, dst=%u)",
                       slot0->srcPhy, slot0->dstPhy);
        return false;
    }

    auto encode = [](auto const& e) -> uint16_t {
        return static_cast<uint16_t>(
            (1U << 7U) | (e.dstPhy << 5U) | (e.srcPhy << 3U));
    };

    auto& seq = context.hwAccess->GetDynamicSequence();
    auto* i2c = seq.i2cBuilder(context.basicConfig.i2cAddress);
    if (slot0.has_value()) {
        i2c->write(REG_PHY_CPY0, encode(slot0.value()));
    }
    if (slot1.has_value()) {
        i2c->write(REG_PHY_CPY1, encode(slot1.value()));
    }

    context.hwAccess->SubmitSequence(seq);
    UDDF_LOG_DEBUG(*context.driverServices, "PHY replication enabled");
    return true;
}

bool MAX967XX::StartStreaming(DeserializerContext& context, uint8_t linkMask) {
    UDDF_LOG_DEBUG(*context.driverServices, "StartStreaming called");

    // Check CSI PLL Lock
    if (m_config.mipiOutputMode ==
        uddf::cdd::MipiOutputMode::CDI_MAX967XX_MIPI_OUT_2x4) {
        context.hwAccess->SubmitSequence(max967xx::hsl::check_csi_pll_lock_2x4);
    } else if (m_config.mipiOutputMode ==
        uddf::cdd::MipiOutputMode::CDI_MAX967XX_MIPI_OUT_4x2) {
        context.hwAccess->SubmitSequence(max967xx::hsl::check_csi_pll_lock_4x2);
    } else {
        UDDF_LOG_ERROR(*context.driverServices, "Unsupported MIPI output mode");
        return false;
    }


    // Trigger deskew for DPHY mode with MIPI speed >= 1.5GHz (1500000 kHz)
    // Index 0 = 2x4 mode, Index 1 = 4x2 mode
    uint8_t rateIndex = (m_config.mipiOutputMode == MipiOutputMode::CDI_MAX967XX_MIPI_OUT_4x2) ? 1U : 0U;
    if (context.basicConfig.phyMode == PhyMode::DPHY &&
        context.basicConfig.dphyRate[rateIndex] >= 1500000U) {
        context.hwAccess->SubmitSequence(max967xx::hsl::trigger_deskew_all_controllers);
    }

    // Call CheckLinkLock
    auto result = CheckLinkLock(context, linkMask);
    if (!result) {
        return false;
    }

    if (!ConfigurePhyReplication(context)) {
        return false;
    }

    // Enable CSI OUT
    context.hwAccess->SubmitSequence(max967xx::hsl::enable_csi_out);

    // Check and Clear if ERRB set
    max967xx::hsl::ErrbData errbData = {
        .ERRBData = {{0x00}},
    };
    context.hwAccess->SubmitSequence(max967xx::hsl::read_errb, errbData);
    if (errbData.ERRBData[0] & 0x04) {
        UDDF_LOG_ERROR(*context.driverServices, "NVDEBUG: ERRB_RX_EN_A is set");
        // TODO: Clear the error
    }

    return true;
}

bool MAX967XX::StopStreaming(DeserializerContext& context, uint8_t linkMask) {
    UDDF_LOG_DEBUG(*context.driverServices, "StopStreaming called");
    /* TODO: Check this if any implementation is needed */
    return true;
}

bool MAX967XX::Deinit(DeserializerContext& context) {
    UDDF_LOG_DEBUG(*context.driverServices, "Deinit called");
    /* TODO: Check this if any implementation is needed */
    return true;
}

bool MAX967XX::Reset(DeserializerContext& context) {
    UDDF_LOG_DEBUG(*context.driverServices, "Reset called");
    /* TODO: Check this if any implementation is needed */
    return true;
}

bool MAX967XX::GetErrorSize(DeserializerContext& context, size_t& maxErrorSize) {
    UDDF_LOG_DEBUG(*context.driverServices, "GetErrorSize called");
    return true;
}

bool MAX967XX::GetErrorInfo(DeserializerContext& context,
    uint8_t* buffer, size_t bufferSize, size_t& actualSize,
    bool& isRemoteError, uint8_t& linkErrorMask) {
    UDDF_LOG_DEBUG(*context.driverServices, "GetErrorInfo called");
    return true;
}

uint32_t MAX967XX::UpdateTxPortConfiguration(DeserializerContext& context) {
    uint32_t desI2CPort = context.basicConfig.desI2CPort;
    DeserializerContext::CsiPortType csiPort = context.basicConfig.csiPort;
    uint32_t txPort = UINT32_MAX;

    // Auto-derive txPort from desI2CPort based on CSI interface type
    // This matches the legacy behavior in UpdateCtxConfiguration_CNvMMax967XXFusa
    switch (csiPort) {
        case DeserializerContext::CsiPortType::CSI_PORT_A:
        case DeserializerContext::CsiPortType::CSI_PORT_C:
        case DeserializerContext::CsiPortType::CSI_PORT_E:
        case DeserializerContext::CsiPortType::CSI_PORT_G:
            // 4x2 mode - single ports (A/C/E/G)
            txPort = (desI2CPort == 0) ?
                static_cast<uint32_t>(TxPortPhy::PHY_C) :
                static_cast<uint32_t>(TxPortPhy::PHY_E);
            break;

        case DeserializerContext::CsiPortType::CSI_PORT_B:
        case DeserializerContext::CsiPortType::CSI_PORT_D:
        case DeserializerContext::CsiPortType::CSI_PORT_F:
        case DeserializerContext::CsiPortType::CSI_PORT_H:
            // 4x2 mode - single ports (B/D/F/H)
            txPort = (desI2CPort == 0) ?
                static_cast<uint32_t>(TxPortPhy::PHY_D) :
                static_cast<uint32_t>(TxPortPhy::PHY_F);
            break;

        case DeserializerContext::CsiPortType::CSI_PORT_AB:
        case DeserializerContext::CsiPortType::CSI_PORT_CD:
        case DeserializerContext::CsiPortType::CSI_PORT_EF:
        case DeserializerContext::CsiPortType::CSI_PORT_GH:
            // 2x4 mode - dual ports (AB/CD/EF/GH)
            txPort = (desI2CPort == 0) ?
                static_cast<uint32_t>(TxPortPhy::PHY_D) :
                static_cast<uint32_t>(TxPortPhy::PHY_E);
            break;

        default:
            UDDF_LOG_WARNING(*context.driverServices,
                "Unknown CSI port type %u, defaulting to CSI-GH logic",
                static_cast<uint32_t>(csiPort));
            txPort = (desI2CPort == 0) ?
                static_cast<uint32_t>(TxPortPhy::PHY_D) :
                static_cast<uint32_t>(TxPortPhy::PHY_E);
            break;
    }

    return txPort;
}

uint32_t MAX967XX::TranslateTxPortToPhyValue(uint32_t txPortConfig) {
    switch (txPortConfig) {
        case 0:
            return static_cast<uint32_t>(TxPortPhy::PHY_C);
        case 1:
            return static_cast<uint32_t>(TxPortPhy::PHY_D);
        case 2:
            return static_cast<uint32_t>(TxPortPhy::PHY_E);
        case 3:
            return static_cast<uint32_t>(TxPortPhy::PHY_F);
        default:
            // Invalid value, default to PHY_D
            return static_cast<uint32_t>(TxPortPhy::PHY_D);
    }
}

uint8_t MAX967XX::CalculatePipelineMappingValue(uint32_t txPort, bool isEmbDataType) {
    // Every mapping has 2 bits for the controller routing, as txPort = [0-3]
    if (isEmbDataType) {
        // Enable 4 mappings: FS, FE, PIX, EMB
        return static_cast<uint8_t>(
            ((txPort << 6U) | (txPort << 4U) | (txPort << 2U) | txPort) & 0xFFU);
    } else {
        // Enable 3 mappings: FS, FE, PIX
        return static_cast<uint8_t>(
            ((txPort << 4U) | (txPort << 2U) | txPort) & 0xFFU);
    }
}

bool MAX967XX::IsDualSensorConfig(const DeserializerContext& context, uint8_t linkIndex) const {
    // D457: one DS5 ASIC emits depth(VC0)+RGB(VC1) on ONE GMSL stream demuxed by VC, NOT two
    // GMSL streams like HAWK. Force the single-sensor pipe/mapping path (pipe N for link N)
    // everywhere; the single-sensor HSL map is patched to demux both VCs.
    (void)context; (void)linkIndex;
    return false;
}

bool MAX967XX::GetCsiDataTypeByte(ImageFormatDataType dataType,
                                   uint8_t& dataTypeByte) const {
    switch (dataType) {
        case ImageFormatDataType::RAW10:
            dataTypeByte = 0x2BU;
            return true;
        case ImageFormatDataType::RAW12:
            dataTypeByte = 0x2CU;
            return true;
        default:
            return false;
    }
}

bool MAX967XX::ConfigureDualSensorPipelineMappingForLink(
    DeserializerContext& context, uint8_t linkIndex, uint32_t txPort) {
    // Full dual-sensor pipeline mapping implementation for all 4 links.
    // Each GMSL link uses TWO consecutive pipes:
    //   Link A → pipes 0,1 (PIPE_SEL 0xF0)
    //   Link B → pipes 2,3 (PIPE_SEL 0xF1)
    //   Link C → pipes 4,5 (PIPE_SEL 0xF2)  — MAX96712 only (8 pipes)
    //   Link D → pipes 6,7 (PIPE_SEL 0xF3)  — MAX96712 only (8 pipes)
    //
    // Subclasses should override to restrict the number of supported links
    // if the hardware has fewer pipes (e.g. MAX96724: 4 pipes, links A & B only).

    // Pipe selection constants - Stream ID (X, Y, Z, U) for GMSL2_6GBPS mode.
    constexpr uint8_t PIPE_X = 0x00U; // Stream ID X
    constexpr uint8_t PIPE_Y = 0x01U; // Stream ID Y
    // constexpr uint8_t PIPE_Z = 0x02U; // not used currently
    // constexpr uint8_t PIPE_U = 0x03U; // not used currently

    // In each 4-bit pipe selection setting, high 2 bits to select GMSL link A, B, C or D;
    // low 2 bits to select stream ID X, Y, Z or U in the selected link
    uint8_t const firstPipe  = static_cast<uint8_t>((linkIndex << 2U) | PIPE_X);
    uint8_t const secondPipe = static_cast<uint8_t>((linkIndex << 2U) | PIPE_Y);

    bool isEmbDataType = linkIndex < MAX_DESERIALIZER_LINKS &&
                         context.initConfig.isEmbeddedDataTypeEnabled[linkIndex];

    // Mapping enable: 0x0F = 4 mappings (with EMB), 0x07 = 3 mappings (without EMB)
    uint8_t const mappingEnable = isEmbDataType ? 0x0FU : 0x07U;

    // Controller routing value from txPort
    // 3 mappings (FS, FE, PIX) without EMB; 4 mappings (FS, FE, PIX, EMB) with EMB
    uint8_t const mapDstValue = CalculatePipelineMappingValue(txPort, isEmbDataType);

    // MIPI CSI-2 data-type byte
    uint8_t dataTypeByte = 0x00U;
    if (!GetCsiDataTypeByte(context.basicConfig.dataType, dataTypeByte)) {
        UDDF_LOG_ERROR(*context.driverServices,
            "Dual-sensor pipeline mapping: unsupported data type %u "
            "for link %u",
            static_cast<uint32_t>(context.basicConfig.dataType),
            linkIndex);
        return false;
    }

    // EMB8 CSI-2 data type (0x12 = embedded_8bit); unused bits are ignored when mapping is disabled
    constexpr uint8_t EMB8_DATA_TYPE = 0x12U;
    uint8_t const embRegByte  = isEmbDataType ? EMB8_DATA_TYPE : 0x00U;

    // Pipe selection value: (secondaryPipe << 4) | primaryPipe
    uint8_t const pipeSelValue = static_cast<uint8_t>((secondPipe << 4U) | firstPipe);

    UDDF_LOG_DEBUG(*context.driverServices,
        "Dual-sensor pipeline mapping link %u: pipes %u,%u, txPort=%u, "
        "dataType=0x%02X, mapDst=0x%02X, pipeSel=0x%02X, embDataEnabled=%d",
        linkIndex, firstPipe, secondPipe, txPort,
        dataTypeByte, mapDstValue, pipeSelValue, isEmbDataType);

    max967xx::hsl::DualSensorPipelineMappingData data = {
        .primary_mapping_enable   = {{ mappingEnable }},
        .primary_map_dst          = {{ mapDstValue }},
        .primary_dt_in            = {{ dataTypeByte }},
        .primary_dt_out           = {{ dataTypeByte }},
        .primary_emb_in           = {{ embRegByte }},
        .primary_emb_out          = {{ embRegByte }},
        .secondary_mapping_enable = {{ mappingEnable }},
        .secondary_map_dst        = {{ mapDstValue }},
        .secondary_dt_in          = {{ dataTypeByte }},
        .secondary_dt_out         = {{ static_cast<uint8_t>(dataTypeByte | 0x40U) }},  // VC1
        .secondary_emb_in         = {{ embRegByte }},
        .secondary_emb_out        = {{ static_cast<uint8_t>(embRegByte | 0x40U) }},    // VC1
        .pipe_sel                 = {{ pipeSelValue }},
    };

    switch (linkIndex) {
        case 0:
            context.hwAccess->SubmitSequence(max967xx::hsl::set_dual_sensor_video_pipeline_mapping_A, data);
            return true;
        case 1:
            context.hwAccess->SubmitSequence(max967xx::hsl::set_dual_sensor_video_pipeline_mapping_B, data);
            return true;
        case 2:
            return doSetDualSensorVideoPipelineMappingLinkC(context, data);
        case 3:
            return doSetDualSensorVideoPipelineMappingLinkD(context, data);
        default:
            UDDF_LOG_ERROR(*context.driverServices,
                "Dual-sensor pipeline mapping: unsupported link %u (valid 0-3)",
                linkIndex);
            return false;
    }
}

bool MAX967XX::ConfigureVideoPipelineMappingForLink(DeserializerContext& context, uint8_t linkIndex) {
    // Update TxPort configuration based on CSI interface and internal I2C port
    uint32_t txPort = UpdateTxPortConfiguration(context);

    // Handle explicit txPort override from JSON configuration
    uint32_t txPortFromJson = context.basicConfig.txPortNum;
    if (txPortFromJson != UINT32_MAX) {
        if (txPortFromJson <= 3) {
            uint32_t originalTxPort = txPort;
            txPort = TranslateTxPortToPhyValue(txPortFromJson);
            UDDF_LOG_DEBUG(*context.driverServices,
                "Using explicit txPort from JSON: config %u -> PHY %u (overrides auto-derived PHY %u)",
                txPortFromJson, txPort, originalTxPort);
        } else {
            UDDF_LOG_ERROR(*context.driverServices,
                "Invalid txPort value %u (valid range: 0-3), using auto-derived PHY %u",
                txPortFromJson, txPort);
        }
    }

    // --- Dual-sensor handling (e.g. HAWK camera module) ---
    // When a link has 2+ sensors, we use the dual-sensor static HSL sequences
    // to configure both pipes.  The register values are computed and passed
    // via the memory block to the link-specific HSL sequence.
    if (IsDualSensorConfig(context, linkIndex)) {
        UDDF_LOG_DEBUG(*context.driverServices,
            "Dual-sensor config detected for link %u (numSensors=%u), using dual-sensor pipeline mapping",
            linkIndex, context.initConfig.numSensorsPerLink[linkIndex]);
        return ConfigureDualSensorPipelineMappingForLink(context, linkIndex, txPort);
    }

    // --- Single-sensor path ---

    // Get the embedded data type flag for this specific link
    bool isEmbDataType = false;
    if (linkIndex < MAX_DESERIALIZER_LINKS) {
        isEmbDataType = context.initConfig.isEmbeddedDataTypeEnabled[linkIndex];
    }

    // Calculate the pipeline mapping register value based on actual PHY txPort and embedded data type
    uint8_t pipelineMappingValue = CalculatePipelineMappingValue(txPort, isEmbDataType);

    // Video output controller value: bits 5:4 denote the DesTxPort (controller/PHY number)
    // PHY_D (txPort=1) = 0x10, PHY_E (txPort=2) = 0x20
    uint8_t videoOutputControllerValue = static_cast<uint8_t>((txPort << 4) & 0xFF);

    UDDF_LOG_DEBUG(*context.driverServices,
        "Setting video pipeline mapping for link %u: txPort PHY=%u, "
        "isEmbDataType=%d, pipelineMapping=0x%02X, outputController=0x%02X",
        linkIndex, txPort, isEmbDataType, pipelineMappingValue, videoOutputControllerValue);

    // Call the appropriate link-specific method with calculated values
    switch (linkIndex) {
        case 0: {
            max967xx::hsl::VideoPipelineMappingDataLinkA pipelineData = {
                .reg_092D = {{ pipelineMappingValue }},
                .reg_0939 = {{ videoOutputControllerValue }},
            };
            context.hwAccess->SubmitSequence(max967xx::hsl::set_ser_video_pipeline_mapping_A, pipelineData);
            break;
        }
        case 1: {
            max967xx::hsl::VideoPipelineMappingDataLinkB pipelineData = {
                .reg_096D = {{ pipelineMappingValue }},
                .reg_0979 = {{ videoOutputControllerValue }},
            };
            context.hwAccess->SubmitSequence(max967xx::hsl::set_ser_video_pipeline_mapping_B, pipelineData);
            break;
        }
        case 2: {
            max967xx::hsl::VideoPipelineMappingDataLinkC pipelineData = {
                .reg_09AD = {{ pipelineMappingValue }},
                .reg_09B9 = {{ videoOutputControllerValue }},
            };
            context.hwAccess->SubmitSequence(max967xx::hsl::set_ser_video_pipeline_mapping_C, pipelineData);
            break;
        }
        case 3: {
            max967xx::hsl::VideoPipelineMappingDataLinkD pipelineData = {
                .reg_09ED = {{ pipelineMappingValue }},
                .reg_09F9 = {{ videoOutputControllerValue }},
            };
            context.hwAccess->SubmitSequence(max967xx::hsl::set_ser_video_pipeline_mapping_D, pipelineData);
            break;
        }
        default:
            UDDF_LOG_ERROR(*context.driverServices, "Unsupported link index for pipeline mapping: 0x%x", linkIndex);
            return false;
    }

    return true;
}

// --- Private helper: I2C port configuration ---
void MAX967XX::SetI2cPort(DeserializerContext& context) {
    uint32_t desI2CPort = context.basicConfig.desI2CPort;

    UDDF_LOG_DEBUG(*context.driverServices,
        "Configuring deserializer I2C port: %u (SoC I2C bus: %u)",
        desI2CPort, context.basicConfig.i2cPortNum);

    switch (desI2CPort) {
        case 0:
            context.hwAccess->SubmitSequence(max967xx::hsl::set_i2c_port0);
            break;
        case 1:
            context.hwAccess->SubmitSequence(max967xx::hsl::set_i2c_port1);
            break;
        default:
            UDDF_LOG_WARNING(*context.driverServices,
                "Unknown deserializer I2C port: %u, using default sequence", desI2CPort);
            context.hwAccess->SubmitSequence(max967xx::hsl::set_i2c_port);
            break;
    }
}

} // namespace uddf::cdd
