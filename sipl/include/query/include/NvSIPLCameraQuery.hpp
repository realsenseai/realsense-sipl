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

#ifndef NVSIPL_CAMERA_QUERY_HPP
#define NVSIPL_CAMERA_QUERY_HPP

#include "NvSIPLCommon.hpp"
#include "NvSIPLCameraTypes.hpp"
#include <memory>
#include <vector>
#include <string>

namespace nvsipl {

/** @brief Defines a list of all external image devices supported by
 * @ref NvSIPLCameraQuery_API and SIPL Device Block drivers. */
struct CameraDeviceInfoList {
    /** Holds a vector of @ref SensorInfo objects. */
    std::vector<SensorInfo> sensorInfoList;
    /** Holds a vector of @ref EEPROMInfo objects. */
    std::vector<EEPROMInfo> eepromInfoList;
    /** Holds a vector of @ref SerInfo objects. */
    std::vector<SerInfo> serInfoList;
    /** Holds a vector of @ref DeserInfo objects. */
    std::vector<DeserInfo> deserInfoList;
    /** Holds a vector of @ref CameraConfig objects for CoE cameras. */
    std::vector<CameraConfig> coeCameraList;
    /** Holds a vector of @ref CameraConfig objects for GMSL cameras. */
    std::vector<CameraConfig> gmslCameraList;
    /** Holds a vector of @ref TransportConfig objects for CoE transport settings. */
    std::vector<CoETransSettings> coeTransportList;
    /** Holds a vector of @ref TransportConfig objects for GMSL transport settings. */
    std::vector<GmslTransSettings> gmslTransportList;
    /** Holds a vector of @ref CameraSystemConfig objects. */
    CameraSystemConfig cameraSystemConfig;
};

/**
 * @class INvSIPLCameraQuery NvSIPLCameraQuery.hpp
 *
 * @brief Defines the public data structures and describes the interfaces
 * for NvSIPLCameraQuery.
 */
class INvSIPLCameraQuery {
public:

    static constexpr uint32_t MAJOR_VER = 1u;
    static constexpr uint32_t MINOR_VER = 0u;
    static constexpr uint32_t PATCH_VER = 0u;

    struct Version {
        uint32_t uMajor = MAJOR_VER;
        uint32_t uMinor = MINOR_VER;
        uint32_t uPatch = PATCH_VER;
    };

    static void GetVersion(Version& version);
    static std::unique_ptr<INvSIPLCameraQuery> GetInstance(void);

    virtual SIPLStatus ParseDatabase(void) = 0;
    virtual SIPLStatus ParseJsonFile(std::string fileName) = 0;
    virtual const CameraDeviceInfoList* GetDeviceInfoList() const = 0;
    virtual SIPLStatus GetCameraSystemConfig(const std::string& name, CameraSystemConfig& sysConfig) const = 0;
    virtual const std::vector<std::string>& GetCameraConfigNames() const = 0;

    virtual ~INvSIPLCameraQuery() = default;
};

} // namespace nvsipl
#endif // NVSIPL_CAMERA_QUERY_HPP