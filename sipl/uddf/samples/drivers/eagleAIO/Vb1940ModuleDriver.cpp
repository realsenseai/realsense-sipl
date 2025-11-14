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

#include "Vb1940ModuleDriver.hpp"

#include <iostream>
#include <string>
#include <array>

#include "uddf/ddi/uuid.hpp"

// Generated file containing HSL bytecode blobs
#include "vb1940_hsl.hpp"

namespace uddf::samples {

using namespace uddf::ddi::interfaces;

namespace {

template<typename... Args>
void log(Args... args) {
    std::cout << "    [Eagle module] ";
    (std::cout << ... << args);
    std::cout << std::endl;
}

} // Anonymous namespace

Vb1940ModuleDriver::Vb1940ModuleDriver() : m_is_streaming{} {
}

Vb1940ModuleDriver::~Vb1940ModuleDriver() {
}

void Vb1940ModuleDriver::LogState(std::string_view label, uddf::cdi::IHardwareAccess& hwAccess) {
    std::string_view system_state {"unknown"};
    uint8_t system_fsm_state {};
    uint8_t boot_fsm_state {};
    uint8_t streaming_fsm_state {};
    constexpr size_t camera_index {};

    if (hwAccess.ReadI2C(camera_index, 0x044U, 1U, &system_fsm_state) &&
        hwAccess.ReadI2C(camera_index, 0x200U, 1U, &boot_fsm_state) &&
        hwAccess.ReadI2C(camera_index, 0x201U, 1U, &streaming_fsm_state)) {

        constexpr size_t system_states {7U};
        std::array<std::string_view, system_states> system_fsm_state_names {{
            "HW_STBY",
            "SYSTEM_UP",
            "BOOT",
            "SW_STBY",
            "STREAMING",
            "STALL",
            "HALT",
        }};
        if (system_fsm_state < system_states) {
            system_state = system_fsm_state_names[system_fsm_state];
        }
    }
    log(label, " ", system_state,
        ", boot_fsm_state: 0x", std::hex, static_cast<uint16_t>(boot_fsm_state),
        ", streaming_fsm_state: 0x", std::hex, static_cast<uint16_t>(streaming_fsm_state)
    );
}

ISensorControl*
Vb1940ModuleDriver::GetSensorControl(const CameraModuleContext& context,
                                  size_t index) {
    log("GetSensorControl called for index: ", index, ".");
    if (index == 0) {
        log("Returning sensor control pointer.");
        return static_cast<ISensorControl*>(&m_sensorDriver);
    }
    log("No such sensor control index (", index, ") found.");
    return nullptr;
}

bool Vb1940ModuleDriver::StartStreaming(const CameraModuleContext& context) {
    LogState("StartStreaming called", *context.hwAccess);
    context.hwAccess->SubmitSequence(vb1940::hsl::start);
    m_is_streaming = true;
    return true;
}

bool Vb1940ModuleDriver::StopStreaming(const CameraModuleContext& context) {
    LogState("StopStreaming called", *context.hwAccess);
    context.hwAccess->SubmitSequence(vb1940::hsl::stop);
    m_is_streaming = false;
    return true;
}

bool Vb1940ModuleDriver::ConfigureDriver(const CoEModuleContext& context, uddf::ddi::DeviceTable& deviceTable) {
    bool have_valid_configuration {};

    log("ConfigureDriver called: CoE configuration (IP: ",
        context.config.ip, ", MAC: ", context.config.mac,
        ", width: ", context.config.width, ", height: ", context.config.height,
        ", frameRate: ", context.config.frameRate, ", pixelFormat: ", context.config.pixelFormat, ")");

    deviceTable.push_back({0x10, 2, 1, 0, 0}); // vb1940 module

    // the only valid configuration
    have_valid_configuration = have_valid_configuration || (
        (context.config.width       == 2560U) &&
        (context.config.height      == 1984U) &&
        (context.config.frameRate   == 30U) &&
        (context.config.pixelFormat == uddf::ddi::PixPackingType::PIX_PACK_RAW10)
    );

    return have_valid_configuration;
}

bool Vb1940ModuleDriver::ProbeHardware(const CoEModuleContext& context) {
    LogState("ProbeHardware called", *context.hwAccess);
    context.hwAccess->SubmitSequence(vb1940::hsl::verify_model);
    return true;
}

bool Vb1940ModuleDriver::Init(const CoEModuleContext& context) {
    LogState("Init called", *context.hwAccess);
    context.hwAccess->SubmitSequence(vb1940::hsl::secure_boot);
    context.hwAccess->SubmitSequence(vb1940::hsl::vt_patch);
    context.hwAccess->SubmitSequence(vb1940::hsl::m2560X1984_30fps);
    LogState("Init complete", *context.hwAccess);
    return true;
}

bool Vb1940ModuleDriver::Deinit(const CoEModuleContext& context) {
    LogState("Deinit called", *context.hwAccess);
    if (m_is_streaming)
    {
        context.hwAccess->SubmitSequence(vb1940::hsl::stop);
        m_is_streaming = false;
    }
    return true;
}

bool Vb1940ModuleDriver::Reset(const CoEModuleContext& context) {
    LogState("Reset called", *context.hwAccess);
    return true;
}

} // namespace uddf::samples
