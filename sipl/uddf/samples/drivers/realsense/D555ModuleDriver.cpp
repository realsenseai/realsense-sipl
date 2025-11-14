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

#include "D555ModuleDriver.hpp"

#include <iostream>
#include <string>
#include <array>

#include "uddf/ddi/uuid.hpp"

// Generated file containing HSL bytecode blobs
#include "d555_hsl.hpp"

namespace uddf::samples {

using namespace uddf::ddi::interfaces;

namespace {

template<typename... Args>
void log(Args... args) {
    // Ensure number base resets each call so prior std::hex insertions elsewhere don't leak
    std::cout << std::dec << "    [D555 module] ";
    (std::cout << ... << args);
    // Reset to decimal again for safety
    std::cout << std::dec << std::endl;
}

} // Anonymous namespace

D555ModuleDriver::D555ModuleDriver() : m_is_streaming{} {
}

D555ModuleDriver::~D555ModuleDriver() {
}

ISensorControl*
D555ModuleDriver::GetSensorControl(const CameraModuleContext& context,
                                  size_t index) {
    log("GetSensorControl called for index: ", index, ".");
    if (index == 0) {
        log("Returning sensor control pointer.");
        return static_cast<ISensorControl*>(&m_sensorDriver);
    }
    log("No such sensor control index (", index, ") found.");
    return nullptr;
}

bool D555ModuleDriver::StartStreaming(const CameraModuleContext& context) {
    log("StrartStreaming called. width=", m_config.width,
        " height=", m_config.height,
        " frameRate=", m_config.frameRate,
        " pixelFormat=", static_cast<int>(m_config.pixelFormat), ".");

    // Depth sequences for all supported modes
    if(m_config.width == 640 && m_config.height == 360) {
        if(m_config.frameRate == 30.0f) {
            context.hwAccess->SubmitSequence(d555::hsl::depth_m640X360_30fps);
        } else if(m_config.frameRate == 60.0f) {
            context.hwAccess->SubmitSequence(d555::hsl::depth_m640x360_60fps);
        } else if(m_config.frameRate == 15.0f) {
            context.hwAccess->SubmitSequence(d555::hsl::depth_m640x360_15fps);
        } else if(m_config.frameRate == 5.0f) {
            context.hwAccess->SubmitSequence(d555::hsl::depth_m640x360_5fps);
        } else {
            log("Unsupported frame rate for 640x360: ", m_config.frameRate);
            return false;
        }
    } else if(m_config.width == 896 && m_config.height == 504) {
        if(m_config.frameRate == 30.0f) {
            context.hwAccess->SubmitSequence(d555::hsl::depth_m896x504_30fps);
        } else if(m_config.frameRate == 60.0f) {
            context.hwAccess->SubmitSequence(d555::hsl::depth_m896x504_60fps);
        } else if(m_config.frameRate == 15.0f) {
            context.hwAccess->SubmitSequence(d555::hsl::depth_m896x504_15fps);
        } else if(m_config.frameRate == 5.0f) {
            context.hwAccess->SubmitSequence(d555::hsl::depth_m896x504_5fps);
        } else {
            log("Unsupported frame rate for 896x504: ", m_config.frameRate);
            return false;
        }
    } else if(m_config.width == 1280 && m_config.height == 720) {
        if(m_config.frameRate == 30.0f) {
            context.hwAccess->SubmitSequence(d555::hsl::depth_m1280X720_30fps);
        } else if(m_config.frameRate == 15.0f) {
            context.hwAccess->SubmitSequence(d555::hsl::depth_m1280x720_15fps);
        } else if(m_config.frameRate == 5.0f) {
            context.hwAccess->SubmitSequence(d555::hsl::depth_m1280x720_5fps);
        } else {
            log("Unsupported frame rate for 1280x720: ", m_config.frameRate);
            return false;
        }
    } else if(m_config.width == 448 && m_config.height == 252) {
        if(m_config.frameRate == 30.0f) {
            context.hwAccess->SubmitSequence(d555::hsl::depth_m448x252_30fps);
        } else if(m_config.frameRate == 60.0f) {
            context.hwAccess->SubmitSequence(d555::hsl::depth_m448x252_60fps);
        } else if(m_config.frameRate == 15.0f) {
            context.hwAccess->SubmitSequence(d555::hsl::depth_m448x252_15fps);
        } else if(m_config.frameRate == 5.0f) {
            context.hwAccess->SubmitSequence(d555::hsl::depth_m448x252_5fps);
        } else {
            log("Unsupported frame rate for 448x252: ", m_config.frameRate);
            return false;
        }
    } else if(m_config.width == 1280 && m_config.height == 800) {
        if(m_config.frameRate == 15.0f) {
            context.hwAccess->SubmitSequence(d555::hsl::depth_m1280x800_15fps);
        } else {
            log("Unsupported frame rate for 1280x800: ", m_config.frameRate);
            return false;
        }
    } else if(m_config.width == 256 && m_config.height == 144) {
        if(m_config.frameRate == 90.0f) {
            context.hwAccess->SubmitSequence(d555::hsl::depth_m256x144_90fps);
        } else {
            log("Unsupported frame rate for 256x144: ", m_config.frameRate);
            return false;
        }
    } else {
        log("Requested stream profile not supported: ", m_config.width, "x", m_config.height);
        return false;
    }
    m_is_streaming = true;
    return true;
}

bool D555ModuleDriver::StopStreaming(const CameraModuleContext& context) {
    log("StopStreaming called.");
    // context.hwAccess->SubmitSequence(d555::hsl::stop);
    m_is_streaming = false;
    return true;
}

bool D555ModuleDriver::ConfigureDriver(const CoEModuleContext& context, uddf::ddi::DeviceTable& deviceTable) {

    log("ConfigureDriver called: CoE configuration (IP: ",
        context.config.ip, ", MAC: ", context.config.mac,
        ", width: ", context.config.width, ", height: ", context.config.height,
        ", frameRate: ", context.config.frameRate, ", pixelFormat: ", context.config.pixelFormat, ")");

    deviceTable.push_back({0x1A, 2, 1, 0, 0}); // D555 module

    m_config = context.config;
    
    return true;
}

bool D555ModuleDriver::ProbeHardware(const CoEModuleContext& context) {
    log("ProbeHardware called.");
    return true;
}

bool D555ModuleDriver::Init(const CoEModuleContext& context) {
    log("Init called.");
    return true;
}

bool D555ModuleDriver::Deinit(const CoEModuleContext& context) {
    log("Deinit called.");
    if (m_is_streaming)
    {
        // context.hwAccess->SubmitSequence(d555::hsl::stop);
        m_is_streaming = false;
    }
    return true;
}

bool D555ModuleDriver::Reset(const CoEModuleContext& context) {
    log("Reset called.");
    return true;
}

bool D555ModuleDriver::setStreamId(const uint8_t stream_id) {
    log("setStreamId called with stream_id: ", stream_id);
    return true;
}

} // namespace uddf::samples
