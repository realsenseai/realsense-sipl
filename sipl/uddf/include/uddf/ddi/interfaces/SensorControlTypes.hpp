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

#ifndef UDDF_DDI_INTERFACES_SENSORCONTROLTYPES_HPP
#define UDDF_DDI_INTERFACES_SENSORCONTROLTYPES_HPP

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace uddf::ddi::interfaces::sensor_control {

// ---- Constants ----
constexpr size_t MAX_FUSE_ID_LENGTH     { 32 };
constexpr size_t MAX_EXPOSURES          { 8 };
constexpr size_t MAX_COLOR_COMPONENTS   { 4 };
constexpr size_t MAX_FRAME_REPORT_BYTES { 4 };
constexpr size_t MAX_SENSOR_CONTEXTS    { 4 };
constexpr size_t MAX_PWL_KNEEPOINTS     { 64 };
constexpr size_t MAX_TEMPERATURES       { 4 };

/**
 * @brief Defines pixel order or color filter array (CFA) patterns.
 */
enum class PixelOrder : uint32_t
{
    NONE = 0, LUMA,
    // YUV formats
    YUV,      YVU,      YUYV,     YVYU,     VYUY,     UYVY,
    XUYV,     XYUV,     VUYX,
    // RGBA formats
    ALPHA,    RGBA,     ARGB,     BGRA,     RG,
    // Bayer formats
    RGGB,     BGGR,     GRBG,     GBRG,
    // RCCC formats
    RCCB,     BCCR,     CRBC,     CBRC,
    RCCC,     CCCR,     CRCC,     CCRC,     CCCC,
    // RGB-IR formats
    BGGI_RGGI, GBIG_GRIG, GIBG_GIRG, IGGB_IGGR,
    RGGI_BGGI, GRIG_GBIG, GIRG_GIBG, IGGR_IGGB,
    // Unknown
    UNKNOWN   /**< Represents an unknown or unsupported pixel order. */
};

/**
 * @brief Holds the range (minimum and maximum) of a sensor attribute.
 */
struct AttributeRange
{
    /** @brief The minimum value of the attribute. */
    float min { 0.0f };

    /** @brief The maximum value of the attribute. */
    float max { 0.0f };
};

/**
 * @brief Holds parameters regarding quantization step sizes for non-HDR sensors.
 */
struct QuantizationStepSize
{
    /**
     * @brief Sensor gain quantization step size.
     * Valid Range: [0.0, 1.0]. Present for non-HDR sensors requiring it.
     */
    std::optional<float> quantizationStepSizeSG {};

    /**
     * @brief Exposure time quantization step size.
     * Valid Range: [0.0, 1.0]. Present for non-HDR sensors requiring it.
     */
    std::optional<float> quantizationStepSizeET {};
};

/**
 * @brief Holds the static attributes of a sensor.
 */
struct SensorAttributes
{
    /** @brief Number of active exposures. Determines valid range in exposure-related arrays below. */
    uint8_t numActiveExposures { 0 };

    /** @brief Sensor name string. May be empty if not supported. */
    std::string sensorName {};

    /** @brief Sensor Color Filter Array (CFA) pattern. */
    PixelOrder sensorCFA { PixelOrder::NONE };

    /** @brief Sensor fuse ID. May be all zeros if not supported. */
    std::array<uint8_t, MAX_FUSE_ID_LENGTH> sensorFuseId {};

    /**
     * @brief Sensor exposure ranges for active exposures. Units: seconds.
     * Only the first numActiveExposures elements are valid.
     * Valid Range: min [0.0, 100.0], max [min, 100.0].
     */
    std::array<AttributeRange, MAX_EXPOSURES> sensorExpRange {};

    /**
     * @brief Sensor gain ranges for active exposures. Unitless gain factor.
     * Only the first numActiveExposures elements are valid.
     * Valid Range: min [0.0, 1000.0], max [min, 1000.0].
     */
    std::array<AttributeRange, MAX_EXPOSURES> sensorGainRange {};

    /**
     * @brief Sensor white balance gain ranges for active exposures.
     * Only the first numActiveExposures elements are valid.
     * Valid Range: min [0.0, 1000.0], max [min, 1000.0].
     */
    std::array<AttributeRange, MAX_EXPOSURES> sensorWhiteBalanceRange {};

    /**
     * @brief Additional sensor gain factor between active exposures.
     * Element `a` holds the sensitivity ratio between capture 0 and capture `a`. Usually 1.
     * Used for calculating HDR ratio. Valid Range: [0.0, 1000.0].
     * Only the first numActiveExposures elements are valid.
     */
    std::array<float, MAX_EXPOSURES> sensorGainFactor {};

    /**
     * @brief Number of frame report bytes supported by the sensor. Zero if not supported.
     * Valid Range: [0, MAX_FRAME_REPORT_BYTES].
     */
    uint32_t numFrameReportBytes { 0 };

    /** @brief Sensor quantization step size parameters (for non-HDR sensors). */
    QuantizationStepSize sensorQuantizationStepSize {};
};

// ---- Control Types ----
using WhiteBalanceGainValues = std::array<float, MAX_COLOR_COMPONENTS>;

struct ExposureGainInfo
{
    struct ExposureArray {
        size_t count { 0 };
        std::array<float, MAX_EXPOSURES> data {};
    };
    std::optional<ExposureArray> exposureTime {};

    struct GainArray {
        size_t count { 0 };
        std::array<float, MAX_EXPOSURES> data {};
    };
    std::optional<GainArray> sensorGain {};
};

struct FrameReportInfo
{
    uint8_t numBytes { 0 };
    std::array<uint8_t, MAX_FRAME_REPORT_BYTES> sensorFrameReport {};
};

struct SensorControls
{
    uint8_t numSensorContexts { 0 };
    struct WbGainArray {
        size_t count { 0 };
        std::array<WhiteBalanceGainValues, MAX_EXPOSURES> data {};
    };
    std::array<std::optional<WbGainArray>, MAX_SENSOR_CONTEXTS> wbControl {};
    std::array<ExposureGainInfo, MAX_SENSOR_CONTEXTS> exposureGainControl {};
    std::optional<FrameReportInfo> frameReportControl {};
    std::optional<bool> illuminationControl {};
};

// ---- Embedded Data Types ----
struct PointDouble2D
{
    double x { 0.0 };
    double y { 0.0 };
};

struct PWLData
{
    uint8_t numKneePoints { 0 };
    std::array<PointDouble2D, MAX_PWL_KNEEPOINTS> kneePoints {};
};

struct CRCData
{
    uint32_t computedCRC { 0 };
    uint32_t embeddedCRC { 0 };
};

struct SensorTempData
{
    uint8_t numTemperatures { 0 };
    std::array<float, MAX_TEMPERATURES> sensorTempCelsius {};
};

struct EmbeddedDataChunk
{
    uint32_t lineLength { 0 };
    const uint8_t* lineData { nullptr };
};

struct EmbeddedDataInfo
{
    uint32_t numExposures { 0 };
    std::optional<ExposureGainInfo> exposureGainInfo {};
    std::optional<SensorControls::WbGainArray> sensorWBGainInfo {};
    std::optional<PWLData> sensorPWLData {};
    std::optional<CRCData> sensorCRCData {};
    std::optional<SensorTempData> sensorTempData {};
    std::optional<uint64_t> frameSequenceNumber {};
    std::optional<uint64_t> frameTimestamp {};
    std::optional<int8_t> errorFlag {};
};

} // namespace uddf::ddi::interfaces::sensor_control

#endif // UDDF_DDI_INTERFACES_SENSORCONTROLTYPES_HPP