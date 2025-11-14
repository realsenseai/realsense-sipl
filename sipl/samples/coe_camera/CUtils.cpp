/*
 * SPDX-FileCopyrightText: Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */

// Standard header files
#include <cstring>
#include <unistd.h>
#include <chrono>
#include <algorithm>
#include <cstdio>
#include <iostream>
#include <errno.h>
#include <sstream>

// Sample application header files
#include "CUtils.hpp"

#define MAX_COE_OVERRIDE_FIELDS 5

using namespace std;

CLogger& CLogger::GetInstance()
{
    static CLogger instance;
    return instance;
}

void CLogger::SetLogLevel(LogLevel level)
{
    m_level = (level > LEVEL_DBG) ? LEVEL_DBG : level;
}

CLogger::LogLevel CLogger::GetLogLevel()
{
    return m_level;
}

void CLogger::SetLogStyle(LogStyle style)
{
    m_style = (style > LOG_STYLE_FUNCTION_LINE) ? LOG_STYLE_FUNCTION_LINE : style;
}

void CLogger::LogLevelMessageVa(LogLevel level,
                                const char *functionName,
                                uint32_t lineNumber,
                                const char *format,
                                va_list ap)
{
    char str[256] = {'\0',};
    if (level > m_level) {
        return;
    }

    strcpy(str, "nvsipl_coe_camera: ");
    switch (level) {
        case LEVEL_NONE:
            break;
        case LEVEL_ERR:
            strcat(str, "ERROR: ");
            break;
        case LEVEL_WARN:
            strcat(str, "WARNING: ");
            break;
        case LEVEL_INFO:
            break;
        case LEVEL_DBG:
            break;
    }

    vsnprintf(str + strlen(str), sizeof(str) - strlen(str), format, ap);

    if (m_style == LOG_STYLE_NORMAL) {
        if ((strlen(str) != 0) && (str[strlen(str) - 1] != '\n')) {
            strcat(str, "\n");
        }
    } else if (m_style == LOG_STYLE_FUNCTION_LINE) {
        if ((strlen(str) != 0) && (str[strlen(str) - 1] == '\n')) {
            str[strlen(str) - 1] = '\0';
        }
        snprintf(str + strlen(str),
                 sizeof(str) - strlen(str),
                 " at %s():%d\n",
                 functionName,
                 lineNumber);
    }

    std::cout << str;
}

void CLogger::LogLevelMessage(LogLevel level,
                              const char *functionName,
                              uint32_t lineNumber,
                              const char *format,
                              ...)
{
    va_list ap;
    va_start(ap, format);
    LogLevelMessageVa(level, functionName, lineNumber, format, ap);
    va_end(ap);
}

void CLogger::LogLevelMessage(LogLevel level,
                              std::string functionName,
                              uint32_t lineNumber,
                              std::string format,
                              ...)
{
    va_list ap;
    va_start(ap, format);
    LogLevelMessageVa(level, functionName.c_str(), lineNumber, format.c_str(), ap);
    va_end(ap);
}

void CLogger::LogMessageVa(const char *format, va_list ap)
{
    char str[128] = {'\0',};
    vsnprintf(str, sizeof(str), format, ap);
    std::cout << str;
}

void CLogger::LogMessage(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    LogMessageVa(format, ap);
    va_end(ap);
}

void CLogger::LogMessage(std::string format, ...)
{
    va_list ap;
    va_start(ap, format);
    LogMessageVa(format.c_str(), ap);
    va_end(ap);
}

void SetSuspendStateLinux()
{
    system("sudo sh -c 'cat > /tmp/suspend_script.sh << \"EOF\"\n"
        "#!/bin/bash\n"
        "truncate -s 0 /var/log/syslog\n"
        "echo N | tee /sys/module/printk/parameters/console_suspend\n"
        "echo 8 | tee /proc/sys/kernel/printk\n"
        "sleep 1\n"
        "echo 1 > /sys/class/tegra_hv_pm_ctl/tegra_hv_pm_ctl/device/trigger_sys_suspend\n"
        "EOF\n"
        "chmod +x /tmp/suspend_script.sh\n"
        "/tmp/suspend_script.sh'");
    sleep(5);
}

SIPLStatus LoadNITOFile(std::string folderPath,
                        std::string moduleName,
                        std::vector<uint8_t>& nito)
{
    // Set up blob file
    const string defaultNitoFilePath = "/var/nvidia/nvcam/settings/sipl/";

    std::string nitoFilePath;
    char const* nitoFilePathFromEnv = getenv("NITO_PATH");

    if (nitoFilePathFromEnv) {
        nitoFilePath = nitoFilePathFromEnv;
    } else if (!folderPath.empty()) {
        nitoFilePath = folderPath;
    } else {
        nitoFilePath = defaultNitoFilePath;
    }

    LOG_INFO("NITO search path: \"%s\"\n", nitoFilePath.c_str());

    string moduleNameLower{};
    for (auto& c : moduleName) {
        moduleNameLower.push_back(std::tolower(c));
    }
    LOG_INFO("Module name lowercase: \"%s\"\n", moduleNameLower.c_str());

    std::string nitoFile[] = {nitoFilePath + moduleName + ".nito",
                              nitoFilePath + moduleNameLower + ".nito"};

    FILE *fp = nullptr;
    std::string openedFilePath;
    for (uint32_t i = 0U; i < sizeof(nitoFile)/sizeof(nitoFile[0]); i++) {
        LOG_INFO("Trying to open: \"%s\"\n", nitoFile[i].c_str());

        // Check if file exists first
        if (access(nitoFile[i].c_str(), F_OK) != 0) {
            LOG_INFO("File does not exist: %s\n", nitoFile[i].c_str());
        }

        fp = fopen(nitoFile[i].c_str(), "rb");
        if (fp == NULL) {
            LOG_INFO("File \"%s\" not found\n", nitoFile[i].c_str());
        } else {
            openedFilePath = nitoFile[i];
            LOG_INFO("Opened NITO file: \"%s\" for module: \"%s\"\n", nitoFile[i].c_str(), moduleName.c_str());
            break;
        }
    }

    if (fp == NULL) {
        LOG_ERR("Unable to open NITO file %s and %s for module \"%s\"\n",
            nitoFile[0].c_str(), nitoFile[1].c_str(), moduleName.c_str());
        return NVSIPL_STATUS_BAD_ARGUMENT;
    }

    // Check file size
    fseek(fp, 0, SEEK_END);
    auto fsize = ftell(fp);
    rewind(fp);

    if (fsize <= 0U) {
        LOG_ERR("Invalid file size: %d bytes\n", fsize);
        LOG_ERR("NITO file for module \"%s\" is of invalid size\n", moduleName.c_str());
        fclose(fp);
        return NVSIPL_STATUS_BAD_ARGUMENT;
    }

    /* allocate blob memory */
    nito.resize(fsize);

    /* load nito */
    LOG_INFO("Reading NITO file data...\n");
    auto load_start_time = std::chrono::high_resolution_clock::now();
    auto result = (long int) fread(nito.data(), 1, fsize, fp);
    auto load_end_time = std::chrono::high_resolution_clock::now();
    auto load_duration = std::chrono::duration_cast<std::chrono::milliseconds>(load_end_time - load_start_time);

    if (result != fsize) {
        LOG_ERR("fread() FAILED!\n");
        LOG_ERR("Expected: %d bytes\n", fsize);
        LOG_ERR("Actually read: %d bytes\n", result);
        LOG_ERR("Error: %s\n", strerror(errno));
        LOG_ERR("Fail to read data from NITO file for module \"%s\"\n", moduleName.c_str());
        nito.resize(0);
        fclose(fp);
        return NVSIPL_STATUS_BAD_ARGUMENT;
    }

    /* close file */
    fclose(fp);
    LOG_INFO("Data from NITO file loaded for module \"%s\"\n", moduleName.c_str());

    return NVSIPL_STATUS_OK;
}

SIPLStatus GetEventName(const NvSIPLPipelineNotifier::NotificationData &event, const char *&eventName)
{
    static const EventMap eventNameTable[] = {
        {NvSIPLPipelineNotifier::NotificationType::NOTIF_INFO_ICP_PROCESSING_DONE,
         "NOTIF_INFO_ICP_PROCESSING_DONE"},
        {NvSIPLPipelineNotifier::NotificationType::NOTIF_INFO_ISP_PROCESSING_DONE,
         "NOTIF_INFO_ISP_PROCESSING_DONE"},
        {NvSIPLPipelineNotifier::NotificationType::NOTIF_INFO_ACP_PROCESSING_DONE,
         "NOTIF_INFO_ACP_PROCESSING_DONE"},
        {NvSIPLPipelineNotifier::NotificationType::NOTIF_INFO_CDI_PROCESSING_DONE,
         "NOTIF_INFO_CDI_PROCESSING_DONE"},
        {NvSIPLPipelineNotifier::NotificationType::NOTIF_INFO_ICP_AUTH_SUCCESS,
         "NOTIF_INFO_ICP_AUTH_SUCCESS"},
        {NvSIPLPipelineNotifier::NotificationType::NOTIF_WARN_ICP_FRAME_DROP,
         "NOTIF_WARN_ICP_FRAME_DROP"},
        {NvSIPLPipelineNotifier::NotificationType::NOTIF_WARN_ICP_FRAME_DISCONTINUITY,
         "NOTIF_WARN_ICP_FRAME_DISCONTINUITY"},
        {NvSIPLPipelineNotifier::NotificationType::NOTIF_WARN_ICP_CAPTURE_TIMEOUT,
         "NOTIF_WARN_ICP_CAPTURE_TIMEOUT"},
        {NvSIPLPipelineNotifier::NotificationType::NOTIF_WARN_ICP_APP_BACK_PRESSURE,
         "NOTIF_WARN_ICP_APP_BACK_PRESSURE"},
        {NvSIPLPipelineNotifier::NotificationType::NOTIF_ERROR_ICP_BAD_INPUT_STREAM,
         "NOTIF_ERROR_ICP_BAD_INPUT_STREAM"},
        {NvSIPLPipelineNotifier::NotificationType::NOTIF_ERROR_ICP_CAPTURE_FAILURE,
         "NOTIF_ERROR_ICP_CAPTURE_FAILURE"},
        {NvSIPLPipelineNotifier::NotificationType::NOTIF_ERROR_ICP_EMB_DATA_PARSE_FAILURE,
         "NOTIF_ERROR_ICP_EMB_DATA_PARSE_FAILURE"},
        {NvSIPLPipelineNotifier::NotificationType::NOTIF_ERROR_ISP_PROCESSING_FAILURE,
         "NOTIF_ERROR_ISP_PROCESSING_FAILURE"},
        {NvSIPLPipelineNotifier::NotificationType::NOTIF_ERROR_ACP_PROCESSING_FAILURE,
         "NOTIF_ERROR_ACP_PROCESSING_FAILURE"},
        {NvSIPLPipelineNotifier::NotificationType::NOTIF_ERROR_ACP_SETTINGS_DISCONTINUITY,
         "NOTIF_ERROR_ACP_SETTINGS_DISCONTINUITY"},
        {NvSIPLPipelineNotifier::NotificationType::NOTIF_ERROR_CDI_SET_SENSOR_CTRL_FAILURE,
         "NOTIF_ERROR_CDI_SET_SENSOR_CTRL_FAILURE"},
        {NvSIPLPipelineNotifier::NotificationType::NOTIF_ERROR_DESERIALIZER_FAILURE,
         "NOTIF_ERROR_DESERIALIZER_FAILURE"},
        {NvSIPLPipelineNotifier::NotificationType::NOTIF_ERROR_SERIALIZER_FAILURE,
         "NOTIF_ERROR_SERIALIZER_FAILURE"},
        {NvSIPLPipelineNotifier::NotificationType::NOTIF_ERROR_SENSOR_FAILURE,
         "NOTIF_ERROR_SENSOR_FAILURE"},
        {NvSIPLPipelineNotifier::NotificationType::NOTIF_ERROR_INTR_LOCALIZATION_FAILURE,
         "NOTIF_ERROR_INTR_LOCALIZATION_FAILURE"},
        {NvSIPLPipelineNotifier::NotificationType::NOTIF_ERROR_INTR_LOCALIZATION_TIMEOUT,
         "NOTIF_ERROR_INTR_LOCALIZATION_TIMEOUT"},
        {NvSIPLPipelineNotifier::NotificationType::NOTIF_ERROR_ICP_AUTH_FAILURE,
         "NOTIF_ERROR_ICP_AUTH_FAILURE"},
        {NvSIPLPipelineNotifier::NotificationType::NOTIF_ERROR_ICP_AUTH_OUT_OF_ORDER,
         "NOTIF_ERROR_ICP_AUTH_OUT_OF_ORDER"},
        {NvSIPLPipelineNotifier::NotificationType::NOTIF_ERROR_INTERNAL_FAILURE,
         "NOTIF_ERROR_INTERNAL_FAILURE"},
        {NvSIPLPipelineNotifier::NotificationType::NOTIF_ERROR_ISP_PREFENCE_TIMEOUT,
         "NOTIF_ERROR_ISP_PREFENCE_TIMEOUT"},
        {NvSIPLPipelineNotifier::NotificationType::NOTIF_INIT_ERROR_FAILURE,
         "NOTIF_INIT_ERROR_FAILURE"}
    };

    for (uint32_t i = 0U; i < sizeof(eventNameTable)/sizeof(eventNameTable[0]); i++) {
        if (event.eNotifType == eventNameTable[i].eventType) {
            eventName = eventNameTable[i].eventName;
            return NVSIPL_STATUS_OK;
        }
    }

    LOG_ERR("Unknown event type\n");
    return NVSIPL_STATUS_BAD_ARGUMENT;
}

SIPLStatus LoadCoEOverrideFile(std::string filePath, std::vector<CoEOverride>& coeOverride)
{
    FILE *fp = fopen(filePath.c_str(), "r");
    if (fp == NULL) {
        LOG_ERR("fopen() failed: %s\n", strerror(errno));
        LOG_ERR("File \"%s\" not found\n", filePath.c_str());
        return NVSIPL_STATUS_BAD_ARGUMENT;
    }

    LOG_INFO("Opened CoE override file: \"%s\"\n", filePath.c_str());

    char line[512];
    int lineNumber = 0;

    while (fgets(line, sizeof(line), fp)) {
        lineNumber++;

        // Skip empty lines and comments (lines starting with #)
        if (line[0] == '\n' || line[0] == '#' || line[0] == '\0') {
            continue;
        }

        // Remove trailing newline
        line[strcspn(line, "\r\n")] = '\0';

        // Parse CSV fields: hsb_string,interface_name,mac_address,ip_address
        char* token;
        char* saveptr;
        CoEOverride override;
        int fieldCount = 0;

        // HSB string
        token = strtok_r(line, ",", &saveptr);
        if (token) {
            override.hsbName = std::string(token);
            fieldCount++;
        }

        // HSB id
        token = strtok_r(NULL, ",", &saveptr);
        if (token) {
            override.hsbId = std::stoi(token);
            fieldCount++;
        }

        // Interface name
        token = strtok_r(NULL, ",", &saveptr);
        if (token) {
            override.interfaceName = std::string(token);
            fieldCount++;
        }

        // MAC address (format: XX:XX:XX:XX:XX:XX)
        token = strtok_r(NULL, ",", &saveptr);
        if (token) {
            std::string input = token;
            std::istringstream ss(input);
            std::string macStr;
            uint32_t i = 0;
            while (std::getline(ss, macStr, ':')) {
                override.macAddress[i++] = std::stoi(macStr, nullptr, 16);
                if (i >= 6) {
                    break;
                }
            }
            fieldCount++;
            PrintMacAddress(override.macAddress, "override MAC:");
        }

        // IP address (format: XXX.XXX.XXX.XXX)
        token = strtok_r(NULL, ",", &saveptr);
        if (token) {
            struct in_addr addr;
            if (inet_aton(token, &addr) == 1) {
                override.ipAddress = addr.s_addr;
                fieldCount++;
                PrintIpAddress(override.ipAddress, "override IP:");
            } else {
                LOG_ERR("Invalid IP address format at line %d: %s\n", lineNumber, token);
                continue;
            }
        }

        if (fieldCount == MAX_COE_OVERRIDE_FIELDS) {
            coeOverride.push_back(override);
            LOG_INFO("Loaded CoE override: HSB=%s, HSB id=%d, Interface=%s\n",
                    override.hsbName.c_str(), override.hsbId, override.interfaceName.c_str());
            PrintMacAddress(override.macAddress, "MAC:");
            PrintIpAddress(override.ipAddress, "IP:");
        } else {
            LOG_ERR("Invalid CSV format at line %d (expected %d fields, got %d)\n",
                    lineNumber, MAX_COE_OVERRIDE_FIELDS, fieldCount);
        }
    }

    fclose(fp);
    LOG_INFO("Loaded %zu CoE override entries from file: %s\n", coeOverride.size(), filePath.c_str());

    return NVSIPL_STATUS_OK;
}

SIPLStatus ApplyCoEOverrides(nvsipl::CameraSystemConfig& config, const std::vector<CoEOverride>& overrides)
{
    LOG_INFO("Applying %zu CoE overrides to camera system configuration\n", overrides.size());

    for (const auto& override : overrides) {
        bool transportFound = false;
        bool cameraFound = false;

        // Apply overrides to transport configurations
        for (auto& transport : config.transports) {
            // Check if this transport is a CoE transport
            if (std::holds_alternative<nvsipl::CoETransSettings>(transport)) {
                nvsipl::CoETransSettings& coeTransport = std::get<nvsipl::CoETransSettings>(transport);

                if (coeTransport.hsbId != override.hsbId) {
                    continue;
                }
                coeTransport.interfaceName = override.interfaceName;
                coeTransport.ipAddress = override.ipAddress;

                LOG_INFO("Applied interface override: %s -> %s\n",
                        coeTransport.name.c_str(), override.interfaceName.c_str());
                PrintIpAddress(override.ipAddress, "Applied IP override:");

                transportFound = true;
            }
        }

        if (!transportFound) {
            LOG_WARN("No matching transport found for override: hsb_id=%d\n",
                override.hsbId);
        }

        for (auto& camera : config.cameras) {
            if (std::holds_alternative<nvsipl::CoECamera>(camera.cameratype)) {
                nvsipl::CoECamera& coeCamera = std::get<nvsipl::CoECamera>(camera.cameratype);
                if (coeCamera.hsbId != override.hsbId) {
                    continue;
                }
                coeCamera.sensors->ipAddress = override.ipAddress;
                for (uint32_t i = 0; i < sizeof(MacAddress); i++) {
                    coeCamera.sensors->macAddress[i] = override.macAddress[i];
                }
                cameraFound = true;
            }
        }

        if (!cameraFound) {
            LOG_WARN("No matching camera found for override: hsb_id=%d\n",
                override.hsbId);
        }
    }

    LOG_INFO("CoE overrides applied successfully\n");
    return NVSIPL_STATUS_OK;
}

void PrintMacAddress(const uint8_t macAddress[6], const std::string& prefixStr)
{
    LOG_INFO("%s %02x:%02x:%02x:%02x:%02x:%02x\n",
            prefixStr.c_str(),
            macAddress[0], macAddress[1],
            macAddress[2], macAddress[3],
            macAddress[4], macAddress[5]);
}

void PrintIpAddress(const uint32_t ipAddress, const std::string& prefixStr)
{
    struct in_addr ipAddr;
    ipAddr.s_addr = ipAddress;
    LOG_INFO("%s %s\n", prefixStr.c_str(), inet_ntoa(ipAddr));
}