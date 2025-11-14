/*
 * SPDX-FileCopyrightText: Copyright (c) 2020-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */

/*  NVIDIA SIPL Control Auto Definitions */

#ifndef NVSIPLCONTROLAUTODEF_HPP
#define NVSIPLCONTROLAUTODEF_HPP

#include "NvSIPLISPStat.hpp"
#include "NvSIPLCDICommon.h"

#include <cstdint>
#include <array>

/**
 * @file
 *
 * @brief <b> NVIDIA SIPL: Auto Control Settings - @ref NvSIPLAutoControl </b>
 *
 */

namespace nvsipl{

/** @addtogroup NvSIPLAutoControl
 * @{
 */

/**
  *  Defines types of SIPL Control Auto plug-ins.
  */
enum PluginType : std::uint8_t {
   NV_PLUGIN = 0,  /** NVIDIA plug-in */
   CUSTOM_PLUGIN0, /** Custom plug-in 0 */
   MAX_NUM_PLUGINS /** Maximum number of plug-ins supported. */
};

/**
 * @brief Sensor settings
 */
struct SiplControlAutoSensorSetting {
    /**
     * Holds the number of sensor contexts to activate. Multiple sensor contexts mode is
     * supported by some sensors, in which multiple set of settings(contexts) are programmed
     * and the sensor toggles between them at runtime. For sensors not supporting this mode
     * of operation, it shall be set to 1.
     * Valid Range: [1, @ref DEVBLK_CDI_MAX_SENSOR_CONTEXTS]
     */
    uint8_t                           numSensorContexts;
    /**
     * Holds the sensor exposure settings to set for each context, supports up to @ref DEVBLK_CDI_MAX_SENSOR_CONTEXTS settings.
     */
    std::array<DevBlkCDIExposure, DEVBLK_CDI_MAX_SENSOR_CONTEXTS> exposureControl;
    /**
     * Holds the sensor white balance settings to set for each context, supports up to @ref DEVBLK_CDI_MAX_SENSOR_CONTEXTS settings.
     */
    std::array<DevBlkCDIWhiteBalance, DEVBLK_CDI_MAX_SENSOR_CONTEXTS> wbControl;
    /**
     * Holds the setting for enabling the IR emitter and turning it ON and OFF for RGB-IR sensors.
     */
    DevBlkCDIIllumination   illuminationControl;
};

/**
 * @brief Parsed frame embedded information.
 */
struct SiplControlEmbedInfo {
    /**
     * Holds the parsed embedded data frame number of exposures info for the captured frame.
     * Valid Range: [1, @ref DEVBLK_CDI_MAX_EXPOSURES]
     */
    uint32_t                          numExposures;
    /**
     * Holds the parsed embedded data sensor exposure info for the captured frame.
     */
    DevBlkCDIExposure                sensorExpInfo;
    /**
     * Holds the parsed embedded data sensor white balance info for the captured frame.
     */
    DevBlkCDIWhiteBalance            sensorWBInfo;
    /**
     * Holds the parsed embedded data sensor temperature info for the captured frame, this variable
     * is not supported in SIPL Control Auto.
     */
    DevBlkCDITemperature             sensorTempInfo;
    /**
     * Holds the parsed embedded data for IR emitter status (ON or OFF) for RGB-IR sensors.
     */
    DevBlkCDIIllumination   illuminationInfo;
};

/**
 * @brief Embedded data and parsed information.
 */
struct SiplControlEmbedData {
    /**
     * Holds the parsed embedded info for the captured frame.
     */
    SiplControlEmbedInfo              embedInfo;
    /**
     * Holds frame sequence number for the captured frame, this variable is not supported in SIPL Control Auto.
     */
    DevBlkCDIFrameSeqNum             frameSeqNum;
    /**
     * Holds information of the embedded data buffer attached to the beginning of the frame, this variable is
     * not supported in SIPL Control Auto.
     */
    DevBlkCDIEmbeddedDataChunk       topEmbeddedData;
    /**
     * Holds information of the embedded data buffer attached to the end of the frame, this variable is not
     * supported in SIPL Control Auto.
     */
    DevBlkCDIEmbeddedDataChunk       bottomEmbeddedData;
};

/**
 * @brief Color Gains assuming order RGGB, RCCB, RCCC.
 */
struct SiplControlAutoAwbGain {
    /**
     * A Boolean flag to control whether white balance gains are valid or not.
     */
    bool                              valid;
    /**
     * Gains that applies to individual color channels
     * Valid Range: [0, 8.0]
     */
    std::array<float_t, NVSIPL_ISP_MAX_COLOR_COMPONENT> gain;
};

/**
 * @brief Automatic white balance settings.
 */
struct SiplControlAutoAwbSetting {
    /**
     * Total white balance gains, including both sensor channel gains and ISP gains
     * Valid Range: [0, 8.0]
     */
    std::array<SiplControlAutoAwbGain, NVSIPL_ISP_MAX_INPUT_PLANES> wbGainTotal;
    /**
     * Correlated color temperature.
     * Valid Range: [2000, 20000]
     */
    float_t                 cct;
    /**
     * Color correction matrix
     * Valid Range: [-8.0, 8.0]
     */
    std::array<std::array<float_t, NVSIPL_ISP_MAX_COLORMATRIX_DIM>, NVSIPL_ISP_MAX_COLORMATRIX_DIM> ccmMatrix;
};

/** @brief Structure containing ISP Stats information.
  */
struct SiplControlIspStatsInfo {
    /**
     * Holds pointers to 2 LAC stats data.
     */
    std::array<NvSiplISPLocalAvgClipStatsData const *, 2> lacData;
    /**
     * Holds pointers to 2 LAC stats settings.
     */
    std::array<NvSiplISPLocalAvgClipStats const *, 2> lacSettings;
    /**
     * Holds pointers to 3 Histogram stats data.
     */
    std::array<NvSiplISPHistogramStatsData const *, 3> histData;
    /**
     * Holds pointers to 3 Histogram stats settings.
     */
    std::array<NvSiplISPHistogramStats const *, 3> histSettings;
};

/**
 * @brief Structure containing metadata info for
 * processing AE/AWB algorithm.
 */
struct SiplControlAutoMetadata {
    /**
     * @brief power factor for statistics compression
     * Valid Range: [0.5, 1.0]
     */
    float_t    alpha;

    /**
     * @brief A Boolean flag for notifying if it is first frame
     * for processing AE/AWB algorithm without statistics.
     * Valid Range: [true, false]
     */
    bool    isFirstFrame;

};

/**
 * @brief Input parameters for processing AE/AWB
 */
struct SiplControlAutoInputParam {
    /**
     * Embedded settings
     */
    SiplControlEmbedData        embedData;
    /**
     * Sensor attributes
     */
    DevBlkCDISensorAttributes  sensorAttr;
    /**
     * Stats buffers and settings
     */
    SiplControlIspStatsInfo     statsInfo;
    /**
     * Metadata info for algorithm
     */
    SiplControlAutoMetadata   autoMetadata;
};

/**
 * @brief AE/AWB Output parameters
 */
struct SiplControlAutoOutputParam {
    /**
     * Sensor exposure and gain settings
     */
    SiplControlAutoSensorSetting    sensorSetting;
    /**
     * AWB settings
     */
    SiplControlAutoAwbSetting       awbSetting;
    /**
     * Digital gain to be applied in ISP
     * Valid Range: [0.0, 8.0]
     */
    float_t                         ispDigitalGain;
};


/** @} */

}  // namespace nvsipl

#endif /* NVSIPLCONTROLAUTODEF_HPP */
