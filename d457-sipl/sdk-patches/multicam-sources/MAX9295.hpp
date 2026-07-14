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

#ifndef UDDF_CDD_MAX9295_HPP
#define UDDF_CDD_MAX9295_HPP

#include "uddf/cdi/IHardwareAccess.hpp"
#include "uddf/cdi/IDriverServices.hpp"
#include "uddf/ddi/interfaces/IGmslSerializer.hpp"
#include "common/SerializerUbb.hpp"

namespace uddf::cdd::max9295 {

using namespace uddf::ddi::interfaces;
using namespace uddf::cdd::gmslubb;

/**
 * @brief Configuration for the MAX9295 serializer.
 *
 * This struct is used to configure the MAX9295 serializer by
 * the module driver.
 */
struct MAX9295Config {
    bool enableEmbDataType {false}; ///< Enable EMB8 data type on the serializer.
    size_t numSensors {0};          ///< Number of sensors connected to the serializer.
};

/**
 * @brief A UBB object for the MAX9295 serializer.
 *
 * This class implements the MAX9295 serializer UBB object for GMSL modules.
 */
class MAX9295 final : public SerializerUbb
{
public:

    /**
     * @brief Construct a new MAX9295 serializer UBB object.
     */
    explicit MAX9295(const GmslModuleContext::Config& config) : SerializerUbb(config) {
        g_serializerInfo.model = "MAX9295";
        // Multi-camera: remember which GMSL link this serializer is on, to program a per-link sensor
        // i2c address translation (link N -> virtual 0x1A + N*0x10). See SerInit().
        m_link = (config.linkIndex <= 3U) ? static_cast<uint8_t>(config.linkIndex) : 0U;
    }

    /**
     * @brief Destroy the MAX9295 serializer UBB object.
     */
    ~MAX9295() = default;

    // --- DeviceUbb Methods ---

    const char* GetName() const override { return "MAX9295"; }
    bool Configure(const GmslModuleContext::Config& config) override;
    uddf::ddi::DeviceTable GetDeviceTable() const override;
    uddf::ddi::GpioPinTable GetGpioPinTable() const override;

    // --- IGmslSerializer Methods ---

    void SerGetInfo(GmslSerializerInfo& info) override;
    bool SerInit(GmslSerializerContext const& context) override;
    bool SerPrepareForModuleInit(GmslSerializerContext const& context) override;
    bool SerFinalizeInit(GmslSerializerContext const& context) override;
    bool SerEnableErrorPin(GmslSerializerContext const& context, bool enable) override;
    bool SerConfigureGPIOForwarding(GmslSerializerContext const& context,
                                    void* gpioForwarding) override;

    /** @brief Set the configuration for the MAX9295 serializer. */
    void SetConfig(const MAX9295Config& config) { m_config = config; }

private:

    GmslSerializerInfo g_serializerInfo;
    bool CheckPresence(GmslSerializerContext const& context);
    MAX9295Config m_config {};
    uint8_t m_link {0U};   ///< GMSL link index (for per-link sensor i2c address translation).

    // MAX9295 specific register addresses
    static constexpr uint16_t DEVICE_ID_REG = 0x000D;
    static constexpr uint16_t DEVICE_REV_REG = 0x000E;
    static constexpr uint8_t  MAX9295A_DEV_ID = 0x91;
    static constexpr uint8_t  MAX9295D_DEV_ID = 0x95;

};

} // namespace uddf::cdd::max9295

#endif // UDDF_CDD_MAX9295_HPP


