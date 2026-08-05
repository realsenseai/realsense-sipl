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

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
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

// Which of the camera's streams to start. The UDDF CoE config carries only
// width/height/frameRate/pixelFormat, and depth and RGB share both resolutions and pixel
// packing (both arrive as RAW16), so the stream cannot be derived from it. Select with the
// D555_STREAM env var, same convention as D457_STREAM elsewhere in this workspace.
enum class StreamKind { Depth, Rgb };

StreamKind RequestedStream() {
    const char* env = std::getenv("D555_STREAM");
    if (env == nullptr) {
        return StreamKind::Depth;
    }
    const std::string_view value{env};
    if (value == "rgb" || value == "RGB" || value == "color") {
        return StreamKind::Rgb;
    }
    if (value == "depth" || value == "DEPTH") {
        return StreamKind::Depth;
    }
    log("Unrecognized D555_STREAM='", env, "'; falling back to depth.");
    return StreamKind::Depth;
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
    const StreamKind stream = RequestedStream();

    log("StrartStreaming called. stream=", (stream == StreamKind::Rgb ? "rgb" : "depth"),
        " width=", m_config.width,
        " height=", m_config.height,
        " frameRate=", m_config.frameRate,
        " pixelFormat=", static_cast<int>(m_config.pixelFormat), ".");

    m_streaming_kind = static_cast<int>(stream);

    if(stream == StreamKind::Rgb) {
        return StartRgbStreaming(context);
    }

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

bool D555ModuleDriver::StartRgbStreaming(const CameraModuleContext& context) {
    // RGB sequences for all supported modes. Profile indices come from
    // RealSense_RGB_Mode in d555_hsl.py; the wire register pair is
    // (offset = profile index, value = streamId<<8 | cmd) with streamId 0 = RGB.
    if(m_config.width == 640 && m_config.height == 360) {
        if(m_config.frameRate == 30.0f) {
            context.hwAccess->SubmitSequence(d555::hsl::rgb_m640X360_30fps);
        } else if(m_config.frameRate == 60.0f) {
            context.hwAccess->SubmitSequence(d555::hsl::rgb_m640x360_60fps);
        } else if(m_config.frameRate == 15.0f) {
            context.hwAccess->SubmitSequence(d555::hsl::rgb_m640x360_15fps);
        } else if(m_config.frameRate == 5.0f) {
            context.hwAccess->SubmitSequence(d555::hsl::rgb_m640x360_5fps);
        } else {
            log("Unsupported RGB frame rate for 640x360: ", m_config.frameRate);
            return false;
        }
    } else if(m_config.width == 896 && m_config.height == 504) {
        if(m_config.frameRate == 30.0f) {
            context.hwAccess->SubmitSequence(d555::hsl::rgb_m896x504_30fps);
        } else if(m_config.frameRate == 60.0f) {
            context.hwAccess->SubmitSequence(d555::hsl::rgb_m896x504_60fps);
        } else if(m_config.frameRate == 15.0f) {
            context.hwAccess->SubmitSequence(d555::hsl::rgb_m896x504_15fps);
        } else if(m_config.frameRate == 5.0f) {
            context.hwAccess->SubmitSequence(d555::hsl::rgb_m896x504_5fps);
        } else {
            log("Unsupported RGB frame rate for 896x504: ", m_config.frameRate);
            return false;
        }
    } else if(m_config.width == 1280 && m_config.height == 720) {
        if(m_config.frameRate == 30.0f) {
            context.hwAccess->SubmitSequence(d555::hsl::rgb_m1280X720_30fps);
        } else if(m_config.frameRate == 15.0f) {
            context.hwAccess->SubmitSequence(d555::hsl::rgb_m1280x720_15fps);
        } else if(m_config.frameRate == 5.0f) {
            context.hwAccess->SubmitSequence(d555::hsl::rgb_m1280x720_5fps);
        } else {
            log("Unsupported RGB frame rate for 1280x720: ", m_config.frameRate);
            return false;
        }
    } else if(m_config.width == 1280 && m_config.height == 800) {
        if(m_config.frameRate == 30.0f) {
            context.hwAccess->SubmitSequence(d555::hsl::rgb_m1280x800_30fps);
        } else if(m_config.frameRate == 15.0f) {
            context.hwAccess->SubmitSequence(d555::hsl::rgb_m1280x800_15fps);
        } else {
            log("Unsupported RGB frame rate for 1280x800: ", m_config.frameRate);
            return false;
        }
    } else if(m_config.width == 448 && m_config.height == 252) {
        if(m_config.frameRate == 30.0f) {
            context.hwAccess->SubmitSequence(d555::hsl::rgb_m448x252_30fps);
        } else if(m_config.frameRate == 60.0f) {
            context.hwAccess->SubmitSequence(d555::hsl::rgb_m448x252_60fps);
        } else if(m_config.frameRate == 15.0f) {
            context.hwAccess->SubmitSequence(d555::hsl::rgb_m448x252_15fps);
        } else if(m_config.frameRate == 5.0f) {
            context.hwAccess->SubmitSequence(d555::hsl::rgb_m448x252_5fps);
        } else {
            log("Unsupported RGB frame rate for 448x252: ", m_config.frameRate);
            return false;
        }
    } else {
        log("Requested RGB stream profile not supported: ", m_config.width, "x", m_config.height);
        return false;
    }
    m_is_streaming = true;
    return true;
}

bool D555ModuleDriver::StopStreaming(const CameraModuleContext& context) {
    log("StopStreaming called.");
    StopStartedStream(context.hwAccess);
    return true;
}

void D555ModuleDriver::StopStartedStream(uddf::cdi::IHardwareAccess* hwAccess) {
    if(!m_is_streaming) {
        return;
    }
    // Stop only the stream this driver started. d555::hsl::stop stops both streams and so
    // underflows the FW start-count of the one that was never started, after which the FW
    // refuses all further profile updates until the camera is rebooted.
    if(m_streaming_kind == static_cast<int>(StreamKind::Rgb)) {
        hwAccess->SubmitSequence(d555::hsl::stop_rgb);
    } else {
        hwAccess->SubmitSequence(d555::hsl::stop_depth);
    }
    m_is_streaming = false;
}

bool D555ModuleDriver::ConfigureDriver(const CoEModuleContext& context, uddf::ddi::DeviceTable& deviceTable) {

    log("ConfigureDriver called: CoE configuration (IP: ",
        context.config.ip, ", MAC: ", context.config.mac,
        ", width: ", context.config.width, ", height: ", context.config.height,
        ", frameRate: ", context.config.frameRate, ", pixelFormat: ", context.config.pixelFormat, ")");

    deviceTable.push_back({0x1A, 2, 0, 0, 0}); // D555 module - sensor index 0

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
    // Belt and braces: StopStreaming normally clears m_is_streaming first, so this only
    // fires if the graph tore down without a StopStreaming.
    StopStartedStream(context.hwAccess);
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
