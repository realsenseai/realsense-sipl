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

#include "Vb1940SensorDriver.hpp"

#include <iostream>
#include <string>

#include "uddf/ddi/uuid.hpp"
#include "uddf/cdi/hal/HSLDynamicSequence.hpp"
#include "uddf/cdi/HSLResult.hpp"
#include "uddf/cdi/hal/II2CBuilder.hpp"
#include "uddf/ddi/interfaces/SensorControlTypes.hpp"
#include "uddf/cdi/IHardwareAccess.hpp"

namespace uddf::samples {

using namespace uddf::ddi::interfaces;

namespace {

template<typename... Args>
void log(Args... args) {
    std::cout << "    [Eagle sensor] ";
    (std::cout << ... << args);
    std::cout << std::endl;
}

static constexpr float EXPOSE_BITS_TO_FLOAT { 100.0F/static_cast<float>(0xFFFFU) };

} // Anonymous namespace

Vb1940SensorDriver::Vb1940SensorDriver() {
}

Vb1940SensorDriver::~Vb1940SensorDriver() {
}

bool Vb1940SensorDriver::GetSensorAttributes(uddf::cdi::IHardwareAccess& hwAccess, SensorAttributes& attributes) const {
    log("GetSensorAttributes called");
    return true;
}

static constexpr uint16_t uint16_from_float(float const value) {
    constexpr uint16_t MAX_VALUE {0xFFFFU};
    uint16_t ret {};
    if (value >= 0.0f) {
        if (value <= static_cast<float>(MAX_VALUE)) {
            ret = static_cast<uint16_t>(value);
        } else {
            ret = MAX_VALUE;
        }
    }
    return ret;
}

bool Vb1940SensorDriver::SetSensorControls(uddf::cdi::IHardwareAccess& hwAccess, const SensorControls& controls) {
    log("SetSensorControls called");
    bool ret {true};
    if (controls.numSensorContexts > 0U) {
        ret = false;
        static constexpr uint32_t MAX_VB1940_CONTEXTS {4U};
        static_assert(uddf::ddi::interfaces::sensor_control::MAX_SENSOR_CONTEXTS >= MAX_VB1940_CONTEXTS, "MAX_SENSOR_CONTEXTS must be at least MAX_VB1940_CONTEXTS");
        static_assert(uddf::ddi::interfaces::sensor_control::MAX_EXPOSURES >= 4U, "MAX_EXPOSURES must be at least 4");
        static_assert(uddf::ddi::interfaces::sensor_control::MAX_COLOR_COMPONENTS >= 4U, "MAX_COLOR_COMPONENTS must be at least 4");
        if (controls.numSensorContexts <= MAX_VB1940_CONTEXTS) {
            uddf::cdi::HSLDynamicSequence& seq = hwAccess.GetDynamicSequence();
            uddf::cdi::II2CBuilder* builder = seq.getI2CBuilder(0U);
            for (uint32_t i = 0; i < controls.numSensorContexts; ++i) {
                uint16_t const base_offset = 0x678U + static_cast<uint16_t>(i * 0x28U);
                const uddf::ddi::interfaces::sensor_control::ExposureGainInfo& exposureGain = controls.exposureGainControl[i];
                if (exposureGain.exposureTime.has_value()) {
                    uddf::ddi::interfaces::sensor_control::ExposureGainInfo::ExposureArray const& exposureTime {exposureGain.exposureTime.value()};
                    if (exposureTime.count >= 1U) {
                        uint16_t const primary_integration_time {uint16_from_float(exposureTime.data[0U] / EXPOSE_BITS_TO_FLOAT)};
                        builder->write(base_offset + 0x02U, static_cast<uint8_t>((primary_integration_time >> 0U) & 0xFFU));
                        builder->write(base_offset + 0x03U, static_cast<uint8_t>((primary_integration_time >> 8U) & 0xFFU));
                    }
                    if (exposureTime.count >= 2U) {
                        uint16_t const ir_integration_time {uint16_from_float(exposureTime.data[1U] / EXPOSE_BITS_TO_FLOAT)};
                        builder->write(base_offset + 0x04U, static_cast<uint8_t>((ir_integration_time >> 0U) & 0xFFU));
                        builder->write(base_offset + 0x05U, static_cast<uint8_t>((ir_integration_time >> 8U) & 0xFFU));
                    }
                    if (exposureTime.count >= 3U) {
                        uint16_t const short_integration_time {uint16_from_float(exposureTime.data[2U] / EXPOSE_BITS_TO_FLOAT)};
                        builder->write(base_offset + 0x06U, static_cast<uint8_t>((short_integration_time >> 0U) & 0xFFU));
                        builder->write(base_offset + 0x07U, static_cast<uint8_t>((short_integration_time >> 8U) & 0xFFU));
                    }
                }
                if (exposureGain.sensorGain.has_value()) {
                    uddf::ddi::interfaces::sensor_control::ExposureGainInfo::GainArray const& sensorGain {exposureGain.sensorGain.value()};
                    if (sensorGain.count >= 1U) {
                        float analog_gain {sensorGain.data[0U]};
                        if (analog_gain < 1.0f) {
                            analog_gain = 1.0f;
                        }
                        if (analog_gain > 4.0f) {
                            analog_gain = 4.0f;
                        }
                        analog_gain = (16.0f*(analog_gain - 1.0f)) / analog_gain;
                        uint8_t const gain_value {static_cast<uint8_t>(analog_gain + 0.5f)};
                        builder->write(base_offset + 0x01U, gain_value);
                    }
                }
                if (controls.wbControl[i].has_value()) {
                    SensorControls::WbGainArray const& wbGain {controls.wbControl[i].value()};
                    if (wbGain.count >= 1U) {
                        uint16_t const digital_gain_r {uint16_from_float(wbGain.data[0U][0U] * 256.0f)};
                        builder->write(base_offset + 0x08U, static_cast<uint8_t>((digital_gain_r >> 0U) & 0xFFU));
                        builder->write(base_offset + 0x09U, static_cast<uint8_t>((digital_gain_r >> 8U) & 0xFFU));

                        uint16_t const digital_gain_g {uint16_from_float(wbGain.data[0U][1U] * 256.0f)};
                        builder->write(base_offset + 0x0AU, static_cast<uint8_t>((digital_gain_g >> 0U) & 0xFFU));
                        builder->write(base_offset + 0x0BU, static_cast<uint8_t>((digital_gain_g >> 8U) & 0xFFU));

                        uint16_t const digital_gain_b {uint16_from_float(wbGain.data[0U][2U] * 256.0f)};
                        builder->write(base_offset + 0x0CU, static_cast<uint8_t>((digital_gain_b >> 0U) & 0xFFU));
                        builder->write(base_offset + 0x0DU, static_cast<uint8_t>((digital_gain_b >> 8U) & 0xFFU));

                        uint16_t const digital_gain_ir {uint16_from_float(wbGain.data[0U][3U] * 256.0f)};
                        builder->write(base_offset + 0x0EU, static_cast<uint8_t>((digital_gain_ir >> 0U) & 0xFFU));
                        builder->write(base_offset + 0x0FU, static_cast<uint8_t>((digital_gain_ir >> 8U) & 0xFFU));
                    }
               }
            }
            uddf::cdi::HSLResult result {hwAccess.SubmitSequence(seq)};
            ret = (result);
            if (!ret) {
                log("Failed to submit sequence");
            }
        }
    }
    return ret;
}

template<typename T, uint32_t offset, T bytes>
static constexpr T read_le(uint8_t const data[]) {
    T ret {};
    for (T i {}; i<bytes; i++) {
        T tmp {static_cast<T>(data[offset + i])};
        tmp <<= static_cast<T>(i << 3U); // shift by multiples of 8
        ret |= tmp;
    }
    return ret;
}

bool Vb1940SensorDriver::ParseTopEmbeddedData(uddf::cdi::IHardwareAccess& hwAccess, const EmbeddedDataChunk& chunk, EmbeddedDataInfo& info) {
    bool ret {};
    if (chunk.lineLength >= 0x200U) {

        // only support 1 exposure
        info.numExposures = 1U;

        // exposure gain
        info.exposureGainInfo = uddf::ddi::interfaces::sensor_control::ExposureGainInfo {
            .exposureTime = uddf::ddi::interfaces::sensor_control::ExposureGainInfo::ExposureArray {
                .count = 3U,
                .data = {{
                    static_cast<float>(read_le<uint16_t, 0x0A4U, 2U>(chunk.lineData)) * EXPOSE_BITS_TO_FLOAT,  // primary integration time
                    static_cast<float>(read_le<uint16_t, 0x0A6U, 2U>(chunk.lineData)) * EXPOSE_BITS_TO_FLOAT,  // IR integration time
                    static_cast<float>(read_le<uint16_t, 0x0A8U, 2U>(chunk.lineData)) * EXPOSE_BITS_TO_FLOAT   // short integration time
                }}
            },
            .sensorGain = uddf::ddi::interfaces::sensor_control::ExposureGainInfo::GainArray {
                .count = 1U,
                .data = {{
                    16.0f / (16.0f - static_cast<float>(chunk.lineData[0x0A2U] & 0x1FU))
                }}
            }
        };

        // wb gain
        info.sensorWBGainInfo = uddf::ddi::interfaces::sensor_control::SensorControls::WbGainArray {
            .count = 1U,
            .data = {{{{
                static_cast<float>(read_le<uint16_t, 0x0AAU, 2U>(chunk.lineData)) / 256.0f,  // wb gain R
                static_cast<float>(read_le<uint16_t, 0x0ACU, 2U>(chunk.lineData)) / 256.0f,  // wb gain G
                static_cast<float>(read_le<uint16_t, 0x0AEU, 2U>(chunk.lineData)) / 256.0f,  // wb gain B
                static_cast<float>(read_le<uint16_t, 0x0B0U, 2U>(chunk.lineData)) / 256.0f   // wb gain IR
            }}}}
        };

        // temperature
        info.sensorTempData = uddf::ddi::interfaces::sensor_control::SensorTempData {
            .numTemperatures = 2U,
            .sensorTempCelsius = {{
                static_cast<float>(read_le<uint16_t, 0x06AU, 2U>(chunk.lineData)),  // sensor temperature 1
                static_cast<float>(read_le<uint16_t, 0x06CU, 2U>(chunk.lineData))   // sensor temperature 2
            }}
        };

        // frame counter
        uint64_t frameCounter { read_le<uint64_t, 0x06EU, 2U>(chunk.lineData) };
        // TODO:  might be able to populate tyhe upper bits
        info.frameSequenceNumber = frameCounter;


        ret = true;
    }
    return ret;
}

bool Vb1940SensorDriver::ParseBottomEmbeddedData(uddf::cdi::IHardwareAccess& hwAccess, const EmbeddedDataChunk& chunk, EmbeddedDataInfo& info) {
    log("ParseBottomEmbeddedData called");
    return true;
}

} // namespace uddf::samples
