/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */

#include "MAX9295.hpp"
#include "MAX9295Hsl.hpp"

#include <chrono>   // std::chrono::milliseconds (multi-camera link-isolation settle delay)
#include <cstdlib>  // getenv (multi-camera A/B toggles for ser translation/reassign)

// MAX9295 SERIALIZER DRIVER
namespace uddf::cdd::max9295 {

using namespace uddf::ddi::interfaces;
using namespace uddf::cdi;

uddf::ddi::DeviceTable MAX9295::GetDeviceTable() const {
    uddf::ddi::DeviceTable deviceTable;
    deviceTable.push_back(uddf::ddi::DeviceTableEntry{
        .i2cAddress = hsl::MAX9295_I2C_ADDRESS_0x40,
        .offsetWidth = 2,
        .dataWidth = 1,
        .flags = 0,
    });
    // MULTI-CAMERA: link>0 serializers need to write the deserializer's link-enable register (REG6 @
    // 0x0006) to isolate their link during address setup (see SerInit/SerFinalizeInit). i2cBuilder is
    // gated to device-table addresses, so register the deser (0x29, same 16-bit-offset/8-bit-data
    // format) here. Only the non-owner links actually issue REG6 writes; link0 never does.
    if (m_link != 0U) {
        deviceTable.push_back(uddf::ddi::DeviceTableEntry{
            .i2cAddress = 0x29,
            .offsetWidth = 2,
            .dataWidth = 1,
            .flags = 0,
        });
        // reassigned ser address (0x40+link) — so we can read it back for diagnostics.
        deviceTable.push_back(uddf::ddi::DeviceTableEntry{
            .i2cAddress = static_cast<uint16_t>(0x40U + m_link),
            .offsetWidth = 2,
            .dataWidth = 1,
            .flags = 0,
        });
    }
    return deviceTable;
}

uddf::ddi::GpioPinTable MAX9295::GetGpioPinTable() const {
    uddf::ddi::GpioPinTable gpioPinTable;
    // No GPIO pins needed for this serializer.
    return gpioPinTable;
}

bool MAX9295::Configure(const GmslModuleContext::Config& config) {
    // log("Configure called");
    return true;
}

void MAX9295::SerGetInfo(GmslSerializerInfo& info) {
    // Note: SerGetInfo doesn't have context access, using simple assignment
    info.model = g_serializerInfo.model;
}

bool MAX9295::CheckPresence(GmslSerializerContext const& context) {
    // Check MAX9295 presence by reading Device ID
    // 0x91 for MAX9295A, 0x95 for MAX9295D
    uint8_t deviceId = 0;
    if (!context.hwAccess->ReadI2C(hsl::MAX9295_I2C_ADDRESS_0x40, DEVICE_ID_REG, 1, &deviceId,
                                    I2CAddressMode::Physical)) {
        UDDF_LOG_ERROR(*context.driverServices, "Failed to read MAX9295 Device ID");
        return false;
    }

    UDDF_LOG_INFO(*context.driverServices, "Device ID: 0x%02x", deviceId);

    if (deviceId == MAX9295A_DEV_ID) {
        UDDF_LOG_INFO(*context.driverServices,
            "MAX9295A detected successfully (Device ID: 0x%02x)", deviceId);
        g_serializerInfo.model = "MAX9295A";
    } else if (deviceId == MAX9295D_DEV_ID) {
        UDDF_LOG_INFO(*context.driverServices,
            "MAX9295D detected successfully (Device ID: 0x%02x)", deviceId);
        g_serializerInfo.model = "MAX9295D";
    } else {
        UDDF_LOG_ERROR(*context.driverServices,
            "Unexpected Device ID. Expected 0x91 (MAX9295A) or 0x95 (MAX9295D), "
            "got 0x%02x", deviceId);
        return false;
    }

    // Read revision ID
    uint8_t revisionId = 0;
    if (!context.hwAccess->ReadI2C(hsl::MAX9295_I2C_ADDRESS_0x40, DEVICE_REV_REG, 1, &revisionId,
                                    I2CAddressMode::Physical)) {
        UDDF_LOG_ERROR(*context.driverServices, "Failed to read MAX9295 Device Revision");
        return false;
    }
    UDDF_LOG_INFO(*context.driverServices, "Device Revision: 0x%02x", revisionId);

    return true;
}

bool MAX9295::SerInit(GmslSerializerContext const& context) {
    UDDF_LOG_INFO(*context.driverServices, "SerInit called");

    if (!CheckPresence(context)) {
        return false;
    }

    // Step 1: Change serializer I2C address from 0x80 to 0x94
    // TODO: Use Virtual address from GmslSerializerConfig.addressTranslations in context

    // MULTI-CAMERA LINK ISOLATION (d4xx max96712_setup_link). For link>0 the serializer's address
    // config (translation reg 0x0044 below, and the reg-0x0000 reassign in SerFinalizeInit) is written
    // to physical 0x40 — but the GMSL control channel broadcasts to EVERY enabled link, and link0's
    // serializer is also at 0x40. Without isolation those writes hit both serializers, corrupting the
    // per-link DS5 translation so the link>0 DS5 (0x2a/0x3a/…) NACKs. Isolate this link on the
    // deserializer (MAX96712 REG6 @ 0x0006 = 0xF0 | (1<<link)) so only THIS link's serializer receives
    // them; re-enabled in SerFinalizeInit after the ser address is made unique. The deser is a local
    // i2c device (0x29) reachable from the serializer's hwAccess. link0 keeps the proven no-isolation
    // path. Ordering (verified on-rig): SerInit → InitWithSerializer → sensor Init →
    // FinalizeInitWithSerializer → SerFinalizeInit, per link, sequentially.
    if (m_link != 0U) {
        auto& hwd = *context.hwAccess;
        uddf::cdi::IHSLDynamicSequence& seqd = hwd.GetDynamicSequence();
        uddf::cdi::II2CBuilder* bd = seqd.i2cBuilder(0x29U, I2CAddressMode::Physical);
        if (bd != nullptr) {
            const uint8_t reg6 = static_cast<uint8_t>(0xF0U | (1U << m_link));
            bd->write(0x0006U, reg6, I2CWriteFlags::NO_READ_VERIFY);   // isolate: only this link
            seqd.delay(std::chrono::milliseconds(20));
            if (std::getenv("D457_ISO_RESET") != nullptr) {
                bd->write(0x0018U, 0x0FU, I2CWriteFlags::NO_READ_VERIFY);  // CTRL1 one-shot reset (d4xx setup_link)
                seqd.delay(std::chrono::milliseconds(100));                // re-lock the isolated link
            }
            hwd.SubmitSequence(seqd);
            UDDF_LOG_INFO(*context.driverServices,
                "MAX9295 link%u: ISOLATE+RESET via deser REG6=0x%02x, CTRL1=0x0F", m_link, reg6);
            // DIAGNOSTIC: is THIS link's serializer reachable at 0x40 now that only it is enabled?
            uint8_t di = 0U;
            const bool ok = static_cast<bool>(context.hwAccess->ReadI2C(
                hsl::MAX9295_I2C_ADDRESS_0x40, DEVICE_ID_REG, 1, &di, I2CAddressMode::Physical));
            UDDF_LOG_INFO(*context.driverServices,
                "MAX9295 link%u: post-isolate ser 0x40 devID read ok=%d val=0x%02x", m_link, ok ? 1 : 0, di);
        } else {
            UDDF_LOG_ERROR(*context.driverServices, "MAX9295 link%u: deser i2cBuilder null (isolate)", m_link);
        }
    }

    // Step 2: Configure I2C translator A for HAWK1 sensor address translation
    // TODO: Use Virtual addresses from GmslSerializerConfig.addressTranslations in context
    // to set the translator A for the sensor addresses, when multiple HAWKs are present.
    context.hwAccess->SubmitSequence(hsl::set_translator_a);

    // MULTI-CAMERA per-link sensor i2c translation. set_translator_a hardcodes the sensor-address
    // translation SOURCE to 0x1A (reg 0x0044 = 0x1A<<1 = 0x34) for EVERY link, so a 2nd D457 on link1
    // — whose DS5 must be reached at 0x2A to avoid colliding with link0's 0x1A on the shared i2c bus —
    // NACKs. Override the translation source per link: link N ser maps virtual (0x1A + N*0x10) -> the
    // DS5 def-addr 0x10 (dst 0x0045 = 0x10<<1 = 0x20, unchanged). link0=0x1A, link1=0x2A, ... . This
    // matches D457Sensor::m_i2cAddr and the nvsipl_camera per-link sensor-address offset.
    if (m_link != 0U && std::getenv("D457_SER_XLAT") != nullptr) {
        const uint8_t srcAddr = static_cast<uint8_t>((0x1AU + m_link * 0x10U) << 1);
        auto& hw = *context.hwAccess;
        uddf::cdi::IHSLDynamicSequence& seq = hw.GetDynamicSequence();
        uddf::cdi::II2CBuilder* b = seq.i2cBuilder(hsl::MAX9295_I2C_ADDRESS_0x40,
                                                   I2CAddressMode::Physical);
        if (b != nullptr) {
            b->write(0x0044U, srcAddr, I2CWriteFlags::NO_READ_VERIFY);
            hw.SubmitSequence(seq);
            UDDF_LOG_INFO(*context.driverServices,
                "MAX9295 link%u: sensor i2c translation 0x0044=0x%02x (virtual 0x%02x -> 0x10)",
                m_link, srcAddr, (0x1AU + m_link * 0x10U));
        } else {
            UDDF_LOG_ERROR(*context.driverServices, "MAX9295 link%u: i2cBuilder null (translation)", m_link);
        }
    }

    /// FIXME: Tracking Jira task: https://jirasw.nvidia.com/browse/L4T-7912
    /// TODO: Add configurability to support for any number of sensors and other data types.
    /// For now, only support 2 sensors for MAX9295D in HAWK module.
    if (m_config.numSensors == 2) {
        // Step 3: Enable video pipeline and clock selection for HAWK module
        context.hwAccess->SubmitSequence(hsl::set_ser_video_phy_clock_max9295d);
        UDDF_LOG_INFO(*context.driverServices,
            "Video pipeline and clock selection enabled for MAX9295D in HAWK module");

        // Step 4: Enable EMB8 data type support if configured by the module driver
        if (m_config.enableEmbDataType) {
            context.hwAccess->SubmitSequence(hsl::set_ser_emb8_max9295d);
            UDDF_LOG_INFO(*context.driverServices,
                "EMB8 enabled for MAX9295D in HAWK module");
        }
    }
    else if (m_config.numSensors == 1) {
        // RealSense D457: single GMSL link, MAX9295A. Apply the known-good single-link
        // video pipe + PHY/clock configuration (ported from the d4xx serializer init).
        context.hwAccess->SubmitSequence(hsl::set_ser_video_phy_clock_max9295a);
        UDDF_LOG_INFO(*context.driverServices,
            "Video pipe + PHY/clock configured for MAX9295A (D457, single link)");
    }
    else {
        UDDF_LOG_ERROR(*context.driverServices,
            "Unsupported numSensors: %zu (supported: 1 for D457/MAX9295A, 2 for HAWK/MAX9295D)",
            m_config.numSensors);
        return false;
    }

    return true;
}

bool MAX9295::SerPrepareForModuleInit(GmslSerializerContext const& context) {
    UDDF_LOG_INFO(*context.driverServices, "SerPrepareForModuleInit called");

    // Step 1: Configure GPIO pins for sensor power
    context.hwAccess->SubmitSequence(hsl::set_gpio_pins_max9295d);

    // // Step 2: Configure IMU GPIO pins
    // TODO: Add submit_sequence for IMU GPIO pins configuration for max9295d

    return true;
}

bool MAX9295::SerFinalizeInit(GmslSerializerContext const& context) {
    UDDF_LOG_INFO(*context.driverServices, "SerFinalizeInit called");

    if (context.config.useExternalFsync) {
        UDDF_LOG_INFO(*context.driverServices,
            "Serializer: configuring EXTERNAL FSYNC (MFP9/MFP10, using pre-compiled HSL sequence)");
        context.hwAccess->SubmitSequence(hsl::set_external_fsync_max9295d);
    } else {
        UDDF_LOG_INFO(*context.driverServices,
            "Serializer: configuring INTERNAL FSYNC (MFP9/MFP10 RX ID=8)");
        context.hwAccess->SubmitSequence(hsl::set_internal_fsync_max9295d);
    }

    // MULTI-CAMERA: reassign this link's serializer to a UNIQUE i2c address (0x40 + link) as the LAST
    // ser step. With multiple links enabled during streaming, two serializers both answering at 0x40
    // collide on the shared tunneled i2c and corrupt the DS5 (0x2a) transaction -> NACK on link>0.
    // MAX9295 self-address is reg 0x0000, bits[7:1]. Done after all ser config (which uses 0x40); the
    // DS5 control path uses the sensor's 0x2a translation (deser tunnel), not the serializer's own addr.
    if (m_link != 0U) {
        {
            const uint8_t newSerAddr = static_cast<uint8_t>((0x40U + m_link) << 1);
            auto& hw = *context.hwAccess;
            uddf::cdi::IHSLDynamicSequence& seq = hw.GetDynamicSequence();
            uddf::cdi::II2CBuilder* b = seq.i2cBuilder(hsl::MAX9295_I2C_ADDRESS_0x40, I2CAddressMode::Physical);
            if (b != nullptr) {
                b->write(0x0000U, newSerAddr, I2CWriteFlags::NO_READ_VERIFY);
                hw.SubmitSequence(seq);
                UDDF_LOG_INFO(*context.driverServices,
                    "MAX9295 link%u: reassigned serializer i2c 0x40 -> 0x%02x", m_link, 0x40U + m_link);
            }
        }

        // END of this link's isolated ser-config window (opened in SerInit): the serializer now has a
        // UNIQUE address (0x40+link), so re-enable it alongside the already-configured lower links. Links
        // are brought up in order 0..N, so links 0..m_link are done -> REG6 = 0xF0 | ((1<<(link+1))-1).
        // The last link's SerFinalizeInit therefore leaves ALL links enabled for streaming.
        auto& hwd = *context.hwAccess;
        uddf::cdi::IHSLDynamicSequence& seqd = hwd.GetDynamicSequence();
        uddf::cdi::II2CBuilder* bd = seqd.i2cBuilder(0x29U, I2CAddressMode::Physical);
        if (bd != nullptr) {
            // D457_ALONE=1: leave ONLY this link enabled (diagnostic — probe link1 DS5 without link0).
            const uint8_t reg6 = (std::getenv("D457_ALONE") != nullptr)
                ? static_cast<uint8_t>(0xF0U | (1U << m_link))
                : static_cast<uint8_t>(0xF0U | ((1U << (m_link + 1U)) - 1U));
            bd->write(0x0006U, reg6, I2CWriteFlags::NO_READ_VERIFY);
            seqd.delay(std::chrono::milliseconds(20));
            if (std::getenv("D457_FINAL_RESET") != nullptr) {
                // d4xx does reset_oneshot with ALL links enabled so every link re-locks together and
                // the RealTek RGB ISP on link0 (glitched by this link's isolation toggle) gets a clean
                // clock before StartStreaming. Reset AFTER re-enabling all lower links.
                bd->write(0x0018U, 0x0FU, I2CWriteFlags::NO_READ_VERIFY);
                seqd.delay(std::chrono::milliseconds(100));
            }
            hwd.SubmitSequence(seqd);
            UDDF_LOG_INFO(*context.driverServices,
                "MAX9295 link%u: RE-ENABLE links via deser REG6=0x%02x", m_link, reg6);
        }
        // AUTHORITATIVE DS5 translation (must be LAST — the SIPL framework programs this link's ser
        // translation from the query DS5 address, which for the replicated -m 0x0011 module is the
        // UN-offset 0x1a; that clobbers link1 with 0x1a->0x10 (colliding with link0) so its real alias
        // 0x2a NACKs. Re-write the correct per-link alias (0x1a+link*0x10 -> 0x10) on THIS link's now-
        // UNIQUE serializer address (0x40+link), so it lands only here and survives as the final value.
        {
            const uint8_t serAddr = static_cast<uint8_t>(0x40U + m_link);
            const uint8_t srcA = static_cast<uint8_t>((0x1AU + m_link * 0x10U) << 1);  // alias<<1
            auto& hwx = *context.hwAccess;
            uddf::cdi::IHSLDynamicSequence& seqx = hwx.GetDynamicSequence();
            uddf::cdi::II2CBuilder* bx = seqx.i2cBuilder(serAddr, I2CAddressMode::Physical);
            if (bx != nullptr) {
                bx->write(0x0044U, srcA, I2CWriteFlags::NO_READ_VERIFY);  // SRC = alias
                bx->write(0x0045U, 0x20U, I2CWriteFlags::NO_READ_VERIFY); // DST = 0x10<<1
                hwx.SubmitSequence(seqx);
            }
            uint8_t t_this = 0U;
            const bool okt = static_cast<bool>(context.hwAccess->ReadI2C(
                serAddr, 0x0044U, 1, &t_this, I2CAddressMode::Physical));
            UDDF_LOG_INFO(*context.driverServices,
                "MAX9295 link%u: FINAL DS5 xlat ser0x%02x[0x44]=0x%02x (want 0x%02x, alias 0x%02x->0x10) rb_ok=%d",
                m_link, serAddr, t_this, srcA, (0x1AU + m_link * 0x10U), okt ? 1 : 0);
        }
    }

    return true;
}

bool MAX9295::SerEnableErrorPin(GmslSerializerContext const& context, bool enable) {
    // TODO: Add MAX9295 specific error pin configuration
    return true;
}

bool MAX9295::SerConfigureGPIOForwarding(GmslSerializerContext const& context, void* gpioForwarding) {
    // TODO: Need to set GPIO forwarding
    // Transport::Init -> Set GPIO Forwarding
    return true;
}

} // namespace uddf::cdd::max9295


