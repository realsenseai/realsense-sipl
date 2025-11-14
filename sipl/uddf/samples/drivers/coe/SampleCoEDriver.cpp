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

#include "SampleCoEDriver.hpp"

#include <iostream>
#include <string>

#include "uddf/ddi/uuid.hpp"

#include "Sequence_Lifecycle.hpp"

namespace uddf::samples {

using namespace uddf::ddi::interfaces;

namespace {

template<typename... Args>
void log(std::string_view variantId, Args... args) {
    std::cout << "    [" << variantId << "] ";
    (std::cout << ... << args);
    std::cout << std::endl;
}

} // Anonymous namespace

SampleCoEDriver::SampleCoEDriver(std::string_view variantId) : m_variantId(variantId) {
    log(m_variantId, "Instance created.");
}

SampleCoEDriver::~SampleCoEDriver() {
    log(m_variantId, "Instance destroyed.");
}

ISensorControl*
SampleCoEDriver::GetSensorControl(const CameraModuleContext& context,
                                  size_t index) {
    log(m_variantId, "GetSensorControl called for index: ", index, ".");
    if (index == 0) {
        log(m_variantId, "Returning sensor control pointer.");
        return static_cast<ISensorControl*>(this);
    }
    log(m_variantId, "No such sensor control index found. Returning nullptr.");
    return nullptr;
}

bool SampleCoEDriver::StartStreaming(const CameraModuleContext& context) {
    log(m_variantId, "StartStreaming called: Simulating starting hardware stream...");
    return true;
}

bool SampleCoEDriver::StopStreaming(const CameraModuleContext& context) {
    log(m_variantId, "StopStreaming called: Simulating stopping hardware stream...");
    return true;
}

bool SampleCoEDriver::ConfigureDriver(const CoEModuleContext& context, uddf::ddi::DeviceTable& deviceTable) {
    log(m_variantId, std::hex, "ConfigureDriver called: Simulating applying CoE configuration (IP: ",
        context.config.ip, ", MAC: ", (uint16_t)context.config.mac[0], ":", (uint16_t)context.config.mac[1], ":etc)...");
    return true;
}

bool SampleCoEDriver::ProbeHardware(const CoEModuleContext& context) {
    log(m_variantId, "ProbeHardware called: Simulating probing hardware...");
    return true;
}

bool SampleCoEDriver::Init(const CoEModuleContext& context) {
    log(m_variantId, "Init called: Simulating CoE control initialization...");
    return true;
}

bool SampleCoEDriver::Deinit(const CoEModuleContext& context) {
    log(m_variantId, "Deinit called: Simulating CoE control deinitialization...");
    return true;
}

bool SampleCoEDriver::Reset(const CoEModuleContext& context) {
    log(m_variantId, "Reset called: Simulating CoE module reset...");
    return true;
}

// --- ISensorControl Methods (Stub Implementations) ---

bool SampleCoEDriver::GetSensorAttributes(uddf::cdi::IHardwareAccess& /*hwAccess*/, SensorAttributes& /*attributes*/) const {
    log(m_variantId, "GetSensorAttributes called (stub).");
    return true;
}

bool SampleCoEDriver::SetSensorControls(uddf::cdi::IHardwareAccess& /*hwAccess*/, const SensorControls& /*controls*/) {
    log(m_variantId, "SetSensorControls called (stub).");
    return true;
}

bool SampleCoEDriver::ParseTopEmbeddedData(uddf::cdi::IHardwareAccess& /*hwAccess*/, const EmbeddedDataChunk& /*chunk*/, EmbeddedDataInfo& /*info*/) {
    log(m_variantId, "ParseTopEmbeddedData called (stub).");
    return true;
}

bool SampleCoEDriver::ParseBottomEmbeddedData(uddf::cdi::IHardwareAccess& /*hwAccess*/, const EmbeddedDataChunk& /*chunk*/, EmbeddedDataInfo& /*info*/) {
    log(m_variantId, "ParseBottomEmbeddedData called (stub).");
    return true;
}

} // namespace uddf::samples
