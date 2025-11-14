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

#include "HsbTransportDriver.hpp"

#include <iostream>
#include <string>
#include <thread>
#include <cstring>
#include <algorithm>
#include <netinet/in.h>
#include <linux/if_ether.h>
#include <linux/if.h>
#include <sstream>
#include <iomanip>
#include <map>
#include <mutex>

/* Hololink interface */
#include <enumerator.hpp>
#include <timeout.hpp>
#include <data_channel.hpp>
#include <hololink.hpp>

#include "uddf/ddi/uuid.hpp"
#include "uddf/ddi/IInterface.hpp"


namespace {

template<typename... Args>
void log(Args... args) {
    std::cout << "    [HSB] ";
    (std::cout << ... << args);
    std::cout << std::endl;
}

#define EXTRA_DEBUG 0
#if EXTRA_DEBUG
#define log_ext(args...) log(args)
#else
#define log_ext(args...)
#endif

} // Anonymous namespace

namespace uddf::samples {

using namespace uddf::ddi::interfaces;
using namespace uddf::cdi;

HsbTransportDriver::HsbTransportDriver()
{
}

HsbTransportDriver::~HsbTransportDriver()
{
}

bool HsbTransportDriver::ConfigureDriver(const CoEBridgeContext& context)
{
    std::string ip_addr = std::to_string((context.basicConfig.hsbIp >> 0) & 0xFF) + "." +
                          std::to_string((context.basicConfig.hsbIp >> 8) & 0xFF) + "." +
                          std::to_string((context.basicConfig.hsbIp >> 16) & 0xFF) + "." +
                          std::to_string((context.basicConfig.hsbIp >> 24) & 0xFF);

    m_hsbId = context.basicConfig.hsbId;

    log("ConfigureDriver - IP ", ip_addr, ", HSB ID ", m_hsbId);

    try {
        hololink::Metadata metadataCam = hololink::Enumerator::find_channel(ip_addr);

        hololink::Metadata overrides;
        overrides.emplace("vsync_enable", int64_t{0});
        overrides.emplace("block_enable", int64_t{0});

        metadataCam.update(overrides);

        hololink::Metadata metadataCam0(metadataCam);
        hololink::DataChannel::use_sensor(metadataCam0, 0U);

        hololink::Metadata metadataCam1(metadataCam);
        hololink::DataChannel::use_sensor(metadataCam1, 1U);

        m_dataChan[0] = std::make_unique<hololink::DataChannel>(metadataCam0);
        m_hololink = m_dataChan[0]->hololink();

        if (m_hololink == nullptr) {
            log("Error: Failed to get hololink object from channel");
            return false;
        }

        m_dataChan[1] = std::make_unique<hololink::DataChannel>(metadataCam1);
        if (m_hololink != m_dataChan[1]->hololink()) {
            log("Error: different hololink object for stereo pair");
            return false;
        }
    }
    catch (const std::exception& e) {
        log("Error configuring driver: ", e.what());
        return false;
    }

    return true;
}

bool HsbTransportDriver::ProbeHardware(const CoEBridgeContext& context)
{
    log("ProbeHardware");

    try {
        log_ext("Starting HSB...");
        m_hololink->start();
        // log_ext("Resetting HSB...");
        // m_hololink->reset();
        return true;
    }
    catch (const std::exception& e) {
        log("Probe failed: ", e.what());
        return false;
    }
}

bool HsbTransportDriver::Init(const CoEBridgeContext& context)
{
    uint32_t sensorMap = context.initConfig.activeSensors;
    const uint32_t chanId0 = context.initConfig.channelNumber[0];
    const uint32_t chanId1 = context.initConfig.channelNumber[1];
    const uint32_t pitchBytes = context.initConfig.resolution.pitch;
    const uint32_t frameSize =
        (context.initConfig.resolution.height +
         context.initConfig.embDataFrontResolution.height +
         context.initConfig.embDataRearResolution.height) * pitchBytes;

    for (uint32_t i = 0U; (i < HSB_MAX_SENSORS) && (sensorMap != 0U); i++) {
        if ((sensorMap & (1U << i)) == 0U) {
            continue;
        }

        sensorMap &= ~(1U << i);

        if (!m_dataChan[i]) {
            log("Error: Data channel ", i, " is not configured");
            return false;
        }

        m_i2c[i] = m_hololink->get_i2c(hololink::CAM_I2C_BUS + i);
        if (!m_i2c[i]) {
            log("Error: Failed to get I2C interface for sensor ", i);
            return false;
        }

        switch (context.initConfig.pixelFormat) {
        case uddf::ddi::PIX_PACK_RAW10:
            m_dataChan[i]->enable_packetizer_10();
            break;
        case uddf::ddi::PIX_PACK_RAW12:
            m_dataChan[i]->enable_packetizer_12();
            break;
        case uddf::ddi::PIX_PACK_RAW16:
            m_dataChan[i]->disable_packetizer();
            break;
        default:
            log("Error: Unsupported pixel format: ", context.initConfig.pixelFormat);
            return false;
        }

        m_dataChan[i]->configure_coe(context.initConfig.channelNumber[i],
                                    frameSize, pitchBytes,
                                    context.basicConfig.vlanEnabled);
    }

    log("Init: Pitch-Height=", pitchBytes, "x", context.initConfig.resolution.height,
        " vlan=", context.basicConfig.vlanEnabled,
        " chan0=", chanId0,
        " chan1=", chanId1,
        " sensorMap=", std::hex, context.initConfig.activeSensors,
        " embFront=", context.initConfig.embDataFrontResolution.height,
        " embRear=", context.initConfig.embDataRearResolution.height);

    try {
        // m_hololink->setup_clock(renesas_bajoran_lite_ts2_clock_config());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } catch (const std::exception& e) {
        log("Error initializing HSB: ", e.what());
        return false;
    }

    return true;
}

bool HsbTransportDriver::SetChannelNumber(const CoEBridgeContext& context, uint32_t channelNumber, size_t cameraIndex)
{
    if ((cameraIndex >= HSB_MAX_SENSORS) ||
        ((context.initConfig.activeSensors & (1U << cameraIndex)) == 0U) ||
        !m_dataChan[cameraIndex]) {
        log("Error: SetChannelNumber - Invalid camera index: ", cameraIndex);
        return false;
    }

    const uint32_t pitchBytes = context.initConfig.resolution.pitch;
    const uint32_t frameSize =
        (context.initConfig.resolution.height +
         context.initConfig.embDataFrontResolution.height +
         context.initConfig.embDataRearResolution.height) * pitchBytes;

    m_dataChan[cameraIndex]->configure_coe(channelNumber, frameSize, pitchBytes,
                                           context.basicConfig.vlanEnabled);

    log("SetChannelNumber channel=", channelNumber, " cameraIndex=", cameraIndex,
        " Pitch x Height=", pitchBytes, "x", context.initConfig.resolution.height,
        " embFront=", context.initConfig.embDataFrontResolution.height,
        " embRear=", context.initConfig.embDataRearResolution.height,
        " vlan=", context.basicConfig.vlanEnabled);

    return true;
}

bool HsbTransportDriver::Deinit(const CoEBridgeContext& context)
{
    std::string ip_addr = std::to_string((context.basicConfig.hsbIp >> 0) & 0xFF) + "." +
                          std::to_string((context.basicConfig.hsbIp >> 8) & 0xFF) + "." +
                          std::to_string((context.basicConfig.hsbIp >> 16) & 0xFF) + "." +
                          std::to_string((context.basicConfig.hsbIp >> 24) & 0xFF);

    log("Deinit - IP ", ip_addr, ", HSB ID ", context.basicConfig.hsbId);

    for (uint32_t i = 0U; i < HSB_MAX_SENSORS; i++) {
        if ((m_dataChan[i] != nullptr) &&
            ((context.initConfig.activeSensors & (1U << i)) != 0U)) {
            m_dataChan[i]->unconfigure();
            m_dataChan[i]->disable_packetizer();
        }
    }

    if (m_hololink) {
        // m_hololink->reset();
        m_hololink->stop();
    }

    // Clear the stored hololink object and DataChannel
    for (auto& i2c : m_i2c) {
        i2c.reset();
    }
    m_hololink.reset();
    for (auto& chan : m_dataChan) {
        chan.reset();
    }

    return true;
}

bool HsbTransportDriver::Reset(const CoEBridgeContext& context)
{
    log("Reset");
    return true;
}

IReadWriteI2C::I2CResult
HsbTransportDriver::ReadI2C(IDriverServices& driverServices,
                            uint8_t sensorIndex, uint16_t address, uint16_t offset,
                            uint8_t* data, uint16_t length)
{

    // TODO: implement when needed
    IReadWriteI2C::I2CResult result = RWI2C_SUCCESS;
    return result;
}

IReadWriteI2C::I2CResult
HsbTransportDriver::WriteI2C(IDriverServices& driverServices,
                             uint8_t sensorIndex, uint16_t address, uint16_t offset,
                             uint8_t const* data, uint16_t length)
{
    hololink::Hololink::I2c *i2cIf = nullptr;
    const uint32_t sensorID = sensorIndex;
    IReadWriteI2C::I2CResult result = RWI2C_SUCCESS;
    log_ext("I2C WRITE: sensor=", std::dec, sensorID, " addr=0x", std::hex, address,
            " reg=0x", offset, " len=", length);

    if (sensorID >= HSB_MAX_SENSORS) {
        log("Error: Unsupported sensor index: ", sensorID);
        return RWI2C_INTERNAL_ERROR;
    }

    i2cIf = m_i2c[sensorID].get();
    if (!i2cIf) {
        log("Error: WriteI2C - not initialized for sensor ", sensorID);
        return RWI2C_INTERNAL_ERROR;
    }

    uint16_t value16 = 0;
    if (data && length >= 2) {
        // MSB first: data[0] = high byte, data[1] = low byte
        value16 = static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8) |
                                        static_cast<uint16_t>(data[1]));
    } else if (data && length == 1) {
        value16 = data[0];
    }


    uint32_t packed = (static_cast<uint32_t>(value16) << 16) | static_cast<uint32_t>(offset);
    log("Packed I2C write value: 0x", std::hex, packed);
    try {
        uint32_t reg_data_buffer = hololink::I2C_CTRL + 16; // Data buffer offset
        m_hololink->write_uint32(reg_data_buffer, packed, nullptr);
    } catch (const std::exception& e) {
        log("I2C write_uint32 failed: ", e.what());
        return RWI2C_INTERNAL_ERROR;
    }

    return result;
}

bool HsbTransportDriver::processEnumeratedDevice(hololink::Metadata& metadata)
{
    // Convert metadata to hsb_info structure
    hsb_info device = {};

    // Get MAC address
    auto hw_addr = metadata.get<std::vector<uint8_t>>("hardware_address");
    if (hw_addr) {
        const size_t mac_len = std::min(hw_addr->size(), static_cast<size_t>(ETH_ALEN));
        memcpy(device.mac, hw_addr->data(), mac_len);
    }

    // Get IP address
    auto peer_ip = metadata.get<std::string>("peer_ip");
    if (peer_ip) {
        strncpy(device.ip_address, peer_ip->c_str(), sizeof(device.ip_address) - 1);
        device.ip_address[sizeof(device.ip_address) - 1] = '\0';
    }

    // Get interface information
    auto interface_index = metadata.get<int64_t>("interface_index");
    if (interface_index) {
        device.interface_index = static_cast<uint32_t>(*interface_index);
    }

    auto interface = metadata.get<std::string>("interface");
    if (interface) {
        strncpy(device.interface_name, interface->c_str(), sizeof(device.interface_name) - 1);
        device.interface_name[sizeof(device.interface_name) - 1] = '\0';
    }

    auto if_addr = metadata.get<std::string>("interface_address");
    if (if_addr) {
        strncpy(device.interface_address, if_addr->c_str(), sizeof(device.interface_address) - 1);
        device.interface_address[sizeof(device.interface_address) - 1] = '\0';
    }

    auto dest_addr = metadata.get<std::string>("destination_address");
    if (dest_addr) {
        strncpy(device.destination_address, dest_addr->c_str(), sizeof(device.destination_address) - 1);
        device.destination_address[sizeof(device.destination_address) - 1] = '\0';
    }

    // Get board ID and control port
    auto board_id = metadata.get<int64_t>("board_id");
    if (board_id) {
        device.board_id = static_cast<uint16_t>(*board_id);
    }

    auto control_port = metadata.get<int64_t>("control_port");
    if (control_port) {
        device.control_port = static_cast<uint32_t>(*control_port);
    }

    // Get firmware versions
    auto cpnx_version = metadata.get<int64_t>("cpnx_version");
    if (cpnx_version) {
        device.cpnx_version = static_cast<uint16_t>(*cpnx_version);
    }

    auto cpnx_crc = metadata.get<int64_t>("cpnx_crc");
    if (cpnx_crc) {
        device.cpnx_crc = static_cast<uint16_t>(*cpnx_crc);
    }

    auto clnx_version = metadata.get<int64_t>("clnx_version");
    if (clnx_version) {
        device.clnx_version = static_cast<uint16_t>(*clnx_version);
    }

    auto clnx_crc = metadata.get<int64_t>("clnx_crc");
    if (clnx_crc) {
        device.clnx_crc = static_cast<uint16_t>(*clnx_crc);
    }

    // Get serial number
    auto serial_number = metadata.get<std::string>("serial_number");
    if (serial_number) {
        // Convert hex string to bytes
        for (size_t i = 0; i < std::min(serial_number->length() / 2, sizeof(device.serial_number)); i++) {
            std::string byte_str = serial_number->substr(i * 2, 2);
            device.serial_number[i] = static_cast<uint8_t>(std::stoi(byte_str, nullptr, 16));
        }
    }

    // Check if the device is already in our list (match by MAC address)
    bool already_found = false;
    for (const auto& dev : m_discovered_devices_transport) {
        if (memcmp(dev.mac, device.mac, ETH_ALEN) == 0) {
            already_found = true;
            break;
        }
    }

    // If device is not already in our list, add it
    if (!already_found) {
        m_discovered_devices_transport.push_back(device);
    }

    // Continue enumeration
    return true;
}

std::vector<hsb_info> HsbTransportDriver::EnumerateDevices(uint32_t timeout_seconds, const char *target_interface)
{
    // Clear previous discovery results
    m_discovered_devices_transport.clear();

    try {
        log("--- Starting Hololink Enumeration (", timeout_seconds, " seconds)",
            target_interface ? " on interface " : "",
            target_interface ? target_interface : "", " ---");

        std::shared_ptr<hololink::Timeout> timeout =
            std::make_shared<hololink::Timeout>(static_cast<float>(timeout_seconds));

        hololink::Enumerator::enumerated(std::bind(&HsbTransportDriver::processEnumeratedDevice, this, std::placeholders::_1), timeout);

        log("--- Enumeration Complete found ", m_discovered_devices_transport.size(), " devices ---");

        // Return a copy of the discovered devices
        return m_discovered_devices_transport;
    }
    catch (const std::exception& e) {
        log("Error during HSB device enumeration: ", e.what());
        return {};  // Return empty vector on error
    }
}

} // namespace uddf::samples
