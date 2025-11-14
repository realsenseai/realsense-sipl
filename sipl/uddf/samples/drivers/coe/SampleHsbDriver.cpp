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

#include "SampleHsbDriver.hpp"

#include <iostream>
#include <string>

#include "uddf/ddi/uuid.hpp"
#include "uddf/cdi/IDriverServices.hpp"


namespace {

template<typename... Args>
void log(Args... args) {
    std::cout << "    [HSB] ";
    (std::cout << ... << args);
    std::cout << std::endl;
}

} // Anonymous namespace

namespace uddf::samples {

using namespace uddf::ddi::interfaces;
using namespace uddf::cdi;

SampleHsbDriver::SampleHsbDriver()
{
}

SampleHsbDriver::~SampleHsbDriver()
{
}

bool SampleHsbDriver::ConfigureDriver(const CoEBridgeContext& context)
{
    log("ConfigureDriver called");
    return true;
}

bool SampleHsbDriver::ProbeHardware(const CoEBridgeContext& context)
{
    log("ProbeHardware called");
    return true;
}

bool SampleHsbDriver::Init(const CoEBridgeContext& context)
{
    log("Init called: ip=", context.initConfig.ipAddress[0],
        ", res=", context.initConfig.resolution.pitch, "x", context.initConfig.resolution.height,
        ", fmt=", context.initConfig.pixelFormat,
        ", activeSensors=", context.initConfig.activeSensors,
        ", syncSensors=", context.initConfig.syncSensors);
    return true;
}

bool SampleHsbDriver::SetChannelNumber(const CoEBridgeContext& context, uint32_t channelNumber, size_t cameraIndex)
{
    log("SetChannelNumber called");
    return true;
}

bool SampleHsbDriver::Deinit(const CoEBridgeContext& context)
{
    log("Deinit called");
    return true;
}

bool SampleHsbDriver::Reset(const CoEBridgeContext& context)
{
    log("Reset called");
    return true;
}

IReadWriteI2C::I2CResult SampleHsbDriver::ReadI2C(uddf::cdi::IDriverServices& driverServices, uint8_t sensorIndex, uint16_t address, uint16_t offset, uint8_t* data, uint16_t length)
{
    static uint8_t readData = 0x00;
    log("Read I2C: sensorIndex = ", (uint16_t)sensorIndex, ", address = 0x", std::hex, address, ", offset = 0x", offset, ", length = 0x", length);
    // Return incrementing values to simulate changing register states for polling operations
    *data = readData++;
    return RWI2C_SUCCESS;
}

IReadWriteI2C::I2CResult SampleHsbDriver::WriteI2C(uddf::cdi::IDriverServices& driverServices, uint8_t sensorIndex, uint16_t address, uint16_t offset, uint8_t const* data, uint16_t length)
{
    log("Write I2C: sensorIndex = ", (uint16_t)sensorIndex, ", address = 0x", std::hex, address, ", offset = 0x", offset, ", length = 0x", length);
    return RWI2C_SUCCESS;
}

} // namespace uddf::samples
