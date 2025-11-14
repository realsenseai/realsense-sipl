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

/* Settings for Renesas Clock generator device */
std::vector<std::vector<uint8_t>> HsbTransportDriver::renesas_bajoran_lite_ts2_clock_config()
{
    return {
        { 0xFC, 0x00, 0x00, 0x00, 0x00 },
        { 0x00, 0x33, 0x10, 0x4A, 0x20, 0x32, 0x02, 0x00, 0x00, 0x04, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 0x10, 0x00, 0x00, 0x19, 0x9A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x00, 0x00, 0x00, 0x00 },
        { 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 0x30, 0x03, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 0x40, 0x03, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 0x50, 0x03, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 0x60, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 0x80, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 0xA0, 0x82, 0x80, 0x36 },
        { 0xA4, 0x00, 0x38, 0x42, 0x5B, 0x10, 0x11, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0x1F, 0x00, 0x00, 0x00, 0x00 },
        { 0xB4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x90, 0x01, 0x00, 0x00 },
        { 0xC4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x55, 0x01, 0xFF, 0x00, 0x1F, 0x00, 0x00, 0x00 },
        { 0xD4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x4A, 0x1E, 0x01, 0x81 },
        { 0xE4, 0x22, 0x00, 0x5C, 0x8F, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0B, 0x01, 0x00, 0x00 },
        { 0xF6, 0x00, 0x00, 0x0D, 0x4D, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 0xFC, 0x00, 0x01, 0x00, 0x00 },
        { 0x00, 0x69, 0x00, 0x0B, 0x6C, 0xB4, 0x03, 0x00, 0x00, 0x84, 0x81, 0x08, 0x66, 0xB4, 0x03, 0x00, 0x00 },
        { 0x10, 0x69, 0x00, 0x0B, 0x6C, 0xB4, 0x03, 0x00, 0x00, 0x69, 0x00, 0x0B, 0x6C, 0xB4, 0x03, 0x00, 0x00 },
        { 0x20, 0x00, 0x00, 0x70, 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 0x30, 0x10, 0x2F, 0x00, 0x02, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 0x40, 0x21, 0x06, 0x44, 0x09 },
        { 0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x61 },
        { 0x55, 0x00, 0x00, 0x05, 0x77, 0x00, 0x04, 0xB7, 0x06, 0x1F, 0x45, 0x0F, 0x04, 0x00, 0x00, 0x7A, 0x80 },
        { 0x65, 0x01, 0x88, 0x00, 0x00, 0x00, 0x00, 0x25, 0x01, 0x00, 0x01, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 0x75, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 0x85, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 0x95, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2C, 0x3B, 0x00, 0x77, 0x70 },
        { 0xA5, 0x80, 0x01, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0 },
        { 0xB5, 0x00, 0xD0, 0x03, 0x00, 0x00, 0x00, 0x00, 0xBA, 0x00, 0x00, 0x00, 0x1A, 0xA6, 0x0F, 0x47, 0x24 },
        { 0xC5, 0x00, 0x24, 0x00, 0x00, 0x11, 0x20, 0x12, 0x0B, 0x10, 0x02, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 0xD5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 0x62, 0x00, 0x7A },
        { 0x62, 0x00, 0x7A },
        { 0x62, 0x00, 0x7A },
        { 0x62, 0x00, 0x7A },
        { 0x62, 0x80, 0x7A },
        { 0x62, 0x00, 0x7A },
        { 0xFC, 0x00, 0x00, 0x00, 0x00 },
        { 0x0A, 0x30 },
        { 0x0A, 0x32 },
        { 0x0A, 0x30 },
        { 0xFC, 0x00, 0x01, 0x00, 0x00 },
        { 0x44, 0x01 },
        { 0xFC, 0x00, 0x00, 0x00, 0x00 }
    };
}

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
        hololink::Metadata metadataCam0 = hololink::Enumerator::find_channel(ip_addr);
        hololink::Metadata metadataCam1(metadataCam0);

        hololink::DataChannel::use_sensor(metadataCam0, 0U);
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
        log_ext("Resetting HSB...");
        m_hololink->reset();
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
        m_hololink->setup_clock(renesas_bajoran_lite_ts2_clock_config());
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
        m_hololink->reset();
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
    hololink::Hololink::I2c *i2cIf = nullptr;
    const uint32_t sensorID = sensorIndex;
    IReadWriteI2C::I2CResult result;

    log_ext("I2C READ: sensor=", std::dec, sensorID, " addr=0x", std::hex, address,
            " reg=0x", offset, " len=", length);

    if (sensorID >= HSB_MAX_SENSORS) {
        log("Error: Unsupported sensor index: ", sensorID);
        return RWI2C_INTERNAL_ERROR;
    }

    i2cIf = m_i2c[sensorID].get();
    if (!i2cIf) {
        log("Error: ReadI2C - not initialized for sensor ", sensorID);
        return RWI2C_INTERNAL_ERROR;
    }

    if (length == 0) {
        log("Warning: Read length is 0");
        return RWI2C_SUCCESS;
    }

    // Prepare write bytes with the register offset (16-bit, big-endian)
    std::vector<uint8_t> write_bytes;
    write_bytes.push_back((offset >> 8) & 0xFF);  // High byte first
    write_bytes.push_back(offset & 0xFF);         // Low byte

    result = executeI2cTransaction(i2cIf, address, write_bytes, length, data);

    return result;
}

IReadWriteI2C::I2CResult
HsbTransportDriver::WriteI2C(IDriverServices& driverServices,
                             uint8_t sensorIndex, uint16_t address, uint16_t offset,
                             uint8_t const* data, uint16_t length)
{
    hololink::Hololink::I2c *i2cIf = nullptr;
    const uint32_t sensorID = sensorIndex;
    IReadWriteI2C::I2CResult result;
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

    // Prepare write bytes with the register offset (16-bit, big-endian) and data
    std::vector<uint8_t> write_bytes;
    write_bytes.push_back((offset >> 8) & 0xFF);  // High byte first
    write_bytes.push_back(offset & 0xFF);         // Low byte
    if (data != nullptr && length > 0) {
        write_bytes.insert(write_bytes.end(), data, data + length);
    }

    result = executeI2cTransaction(i2cIf, address, write_bytes, 0, nullptr);

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

IReadWriteI2C::I2CResult HsbTransportDriver::executeI2cTransaction(
    hololink::Hololink::I2c* const i2cIf,
    uint16_t i2c_dev_addr,
    const std::vector<uint8_t>& bytes_to_write,
    uint16_t num_bytes_to_read,
    uint8_t* read_buf)
{
    if (num_bytes_to_read > 0 && read_buf == nullptr) {
        log("Error: Output buffer is null but read was requested.");
        return RWI2C_INTERNAL_ERROR;
    }

    try {
        std::vector<uint8_t> tmp_read_buffer =
                 i2cIf->i2c_transaction(i2c_dev_addr, bytes_to_write, num_bytes_to_read);

        if (num_bytes_to_read > 0) {
            if (tmp_read_buffer.size() != num_bytes_to_read) {
                log("Error: Expected ", num_bytes_to_read, " bytes but received ", tmp_read_buffer.size(), " bytes from i2c_transaction");
                return RWI2C_INTERNAL_ERROR;
            }
            std::memcpy(read_buf, tmp_read_buffer.data(), num_bytes_to_read);
        }
        return RWI2C_SUCCESS;
    } catch (const std::exception& e) {
        log("Error during I2C transaction: ", e.what(), " addr=0x", std::hex, i2c_dev_addr);
        return RWI2C_ERROR_UNKNOWN;
    }
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
