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

#include <iostream>
#include <iomanip>
#include <string>
#include <memory>
#include <vector>
#include <set>
#include <cstring>
#include <fstream>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>

// Include SIPL APIs (these should include the necessary NvSci headers)
#include "NvSIPLCameraQuery.hpp"
#include "NvSIPLCameraTypes.hpp"

using namespace nvsipl;

// Forward declarations
void PrintCameraConfig(const CameraConfig& config);
void PrintTransportSettings(const TransportConfig& settings);
std::string FormatMAC(const uint8_t* mac);
void PrintMAC(const uint8_t* mac);

// Helper function to format MAC address as string
std::string FormatMAC(const uint8_t* mac) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (int i = 0; i < 6; i++) {
        if (i > 0) ss << ":";
        ss << std::setw(2) << static_cast<int>(mac[i]);
    }
    return ss.str();
}

// Helper function to print MAC address
void PrintMAC(const uint8_t* mac) {
    std::cout << FormatMAC(mac) << std::endl;
}

// Helper function to print camera configuration
void PrintCameraConfig(const CameraConfig& config) {
    // Basic Information - compact single line format
    std::cout << "Platform: " << config.platform << " | Config: " << config.platformConfig
              << " | EEPROM: " << (config.isEEPROMSupported ? "Yes" : "No") << std::endl;

    // MIPI Settings - compact format
    std::cout << "MIPI: " << (config.mipiSettings.phyMode == NVSIPL_CAP_CSI_DPHY_MODE ? "DPHY" : "CPHY");
    if (config.mipiSettings.dphyRate > 0) std::cout << " " << config.mipiSettings.dphyRate << "kbps";
    if (config.mipiSettings.cphyRate > 0) std::cout << " " << config.mipiSettings.cphyRate << "ksps";
    std::cout << std::endl;

    // Camera Type - compact format
    if (std::holds_alternative<CoECamera>(config.cameratype)) {
        const auto& coe = std::get<CoECamera>(config.cameratype);
        struct in_addr ip_addr;
        ip_addr.s_addr = coe.sensors[0].ipAddress;
        std::cout << "CoE: MAC=" << FormatMAC(coe.sensors[0].macAddress)
                  << " | IP=" << (coe.sensors[0].ipAddress ? inet_ntoa(ip_addr) : "None")
                  << " | HSB=" << coe.hsbId << std::endl;
    } else if (std::holds_alternative<GmslCamera>(config.cameratype)) {
        const auto& gmsl = std::get<GmslCamera>(config.cameratype);
        std::cout << "GMSL: Link=" << gmsl.linkIndex << " | CSI=" << gmsl.csiPort;
        if (!gmsl.serInfo.name.empty()) {
            std::cout << " | Ser=" << gmsl.serInfo.name;
        }
        std::cout << std::endl;
    }

    // Sensor Information - compact format
    std::cout << "\n" << "Sensor" << ":" << std::endl;
    std::cout << "Name: " << config.sensorInfo.name << " | ID: " << config.sensorInfo.id
              << " | I2C: 0x" << std::hex << std::setfill('0') << std::setw(2)
              << static_cast<int>(config.sensorInfo.i2cAddress) << std::dec << std::endl;

    std::cout << "Resolution: " << config.sensorInfo.vcInfo.resolution.width
              << "x" << config.sensorInfo.vcInfo.resolution.height
              << " @ " << config.sensorInfo.vcInfo.fps << "fps"
              << " | CFA: " << config.sensorInfo.vcInfo.cfa << std::endl;

    std::cout << "Input Format: " << config.sensorInfo.vcInfo.inputFormat
              << " | Embedded Data: " << (config.sensorInfo.vcInfo.isEmbeddedDataTypeEnabled ? "Yes" : "No") << std::endl;

    if (config.sensorInfo.vcInfo.embeddedTopLines > 0 || config.sensorInfo.vcInfo.embeddedBottomLines > 0) {
        std::cout << "Embedded Lines: Top=" << config.sensorInfo.vcInfo.embeddedTopLines
                  << " | Bottom=" << config.sensorInfo.vcInfo.embeddedBottomLines << std::endl;
    }

    if (!config.eepromInfo.name.empty()) {
        std::cout << "EEPROM: " << config.eepromInfo.name << " | I2C: 0x"
                  << std::hex << std::setfill('0') << std::setw(2)
                  << static_cast<int>(config.eepromInfo.i2cAddress) << std::dec << std::endl;
    }
}

// Helper function to print transport settings
void PrintTransportSettings(const TransportConfig& settings) {
    if (std::holds_alternative<CoETransSettings>(settings)) {
        const auto& coe = std::get<CoETransSettings>(settings);
        struct in_addr ip_addr;
        ip_addr.s_addr = coe.ipAddress;
        std::cout << "CoE Transport | HSB ID: " << coe.hsbId
                  << " | Interface: " << coe.interfaceName
                  << " | IP: " << inet_ntoa(ip_addr)
                  << " | VLAN: " << (coe.vlanEnable ? "Yes" : "No")
                  << " | Sync: " << (coe.syncSensors ? "Yes" : "No") << std::endl;

    } else if (std::holds_alternative<GmslTransSettings>(settings)) {
        const auto& gmsl = std::get<GmslTransSettings>(settings);
        std::cout << "GMSL Transport | CSI Port: " << gmsl.csiPort
                  << " | I2C Device: " << gmsl.i2cDevice;
        if (!gmsl.deserInfo.name.empty()) {
            std::cout << " | Deserializer: " << gmsl.deserInfo.name;
        }
        std::cout << std::endl;

    } else {
        std::cout << "Error: Unknown transport configuration type" << std::endl;
    }
}

static void ShowUsage(const char* argv0, const std::string& arg) {
    std::cout << "Unknown argument: " << arg << std::endl;
    std::cout << "Usage: " << argv0 << " -t <jsonConfigFile>" << std::endl;
    std::cout << "Usage: " << argv0 << " -c <configName>" << std::endl;
    std::cout << "Usage: " << argv0 << " -l" << std::endl;
}

int main(int argc, char* argv[]) {
    std::string configName;
    std::string jsonFile;

    if (argc == 1) {
        ShowUsage(argv[0], "");
        return 0;
    }

    std::string arg = argv[1];
    if (arg != "-l" && argc == 2) {
        std::cout << "No argument provided" << std::endl;
        ShowUsage(argv[0], "");
        return 0;
    }

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-l") {
            std::cout << "List all configs" << std::endl;
        } else if (arg == "-c") {
            configName = argv[++i];
            std::cout << "Config name: " << configName << std::endl;
        } else if (arg == "-t") {
            jsonFile = argv[++i];
            std::cout << "Test config file: " << jsonFile << std::endl;
        } else {
            ShowUsage(argv[0], arg);
            return 0;
        }
    }



    try {
        // Print application header
        std::cout << "--- NVSIPL COE QUERY TEST ---" << std::endl;

        // Create Query instance using the standard API
        std::cout << "Initializing Query API..." << std::endl;
        std::unique_ptr<INvSIPLCameraQuery> query = INvSIPLCameraQuery::GetInstance();

        if (!query) {
            std::cerr << "Failed to create Query instance" << std::endl;
            return 1;
        }

        // Parse database with standard configurations
        auto status = query->ParseDatabase();
        if (status != NVSIPL_STATUS_OK) {
            std::cerr << "Failed to parse database. Status: " << static_cast<int>(status) << std::endl;
            return 1;
        }

        if (!configName.empty()) {
            CameraSystemConfig cameraSystemConfig;
            std::cout << "Parsing config name: " << configName << std::endl;
            std::cout << "------------" << std::endl;
            status = query->GetCameraSystemConfig(configName, cameraSystemConfig);
            if (status != NVSIPL_STATUS_OK) {
                std::cerr << "Failed to parse config name '" << configName
                          << "'. Status: " << static_cast<int>(status) << std::endl;
                return 1;
            }

            for (const auto& camera : cameraSystemConfig.cameras) {
                std::cout << "Camera: " << camera.name << " (ID: " << camera.sensorInfo.id << ")" << std::endl;
                PrintCameraConfig(camera);
                std::cout << "------------" << std::endl;
            }


            for (const auto& transport : cameraSystemConfig.transports) {
                PrintTransportSettings(transport);
                std::cout << "------------" << std::endl;
            }

            return 0;
        }

        // Parse additional JSON file if provided
        if (!jsonFile.empty()) {
            std::cout << "Parsing additional JSON file: " << jsonFile << std::endl;
            status = query->ParseJsonFile(jsonFile);
            if (status != NVSIPL_STATUS_OK) {
                std::cerr << "Failed to parse JSON file '" << jsonFile
                          << "'. Status: " << static_cast<int>(status) << std::endl;
                return 1;
            }
            std::cout << "Successfully parsed JSON file: " << jsonFile << std::endl;
        }

        // Get device info list
        const CameraDeviceInfoList* deviceList = query->GetDeviceInfoList();
        if (deviceList == nullptr || deviceList->cameraSystemConfig.cameras.empty()) {
            std::cerr << "Failed to get device info list" << std::endl;
            return 1;
        }

        std::cout << "------------" << std::endl;
        for (const auto& camera : deviceList->cameraSystemConfig.cameras) {
            std::cout << "Camera: " << camera.name << " (ID: " << camera.sensorInfo.id << ")" << std::endl;
            PrintCameraConfig(camera);
            std::cout << "------------" << std::endl;
        }

        for (const auto& transport : deviceList->cameraSystemConfig.transports) {
            PrintTransportSettings(transport);
            std::cout << "------------" << std::endl;
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
}
