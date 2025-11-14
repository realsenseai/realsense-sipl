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

#include "D555SensorDriver.hpp"

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
    std::cout << "    [D555 sensor] ";
    (std::cout << ... << args);
    std::cout << std::endl;
}

static constexpr float EXPOSE_BITS_TO_FLOAT { 100.0F/static_cast<float>(0xFFFFU) };

} // Anonymous namespace

D555SensorDriver::D555SensorDriver() {
}

D555SensorDriver::~D555SensorDriver() {
}

bool D555SensorDriver::GetSensorAttributes(uddf::cdi::IHardwareAccess& hwAccess, SensorAttributes& attributes) const {
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

bool D555SensorDriver::SetSensorControls(uddf::cdi::IHardwareAccess& hwAccess, const SensorControls& controls) {
    log("SetSensorControls called");
    bool ret {true};
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

bool D555SensorDriver::ParseTopEmbeddedData(uddf::cdi::IHardwareAccess& hwAccess, const EmbeddedDataChunk& chunk, EmbeddedDataInfo& info) {
    bool ret {true};
    log("ParseTopEmbeddedData called");
    return ret;
}

bool D555SensorDriver::ParseBottomEmbeddedData(uddf::cdi::IHardwareAccess& hwAccess, const EmbeddedDataChunk& chunk, EmbeddedDataInfo& info) {
    log("ParseBottomEmbeddedData called");
    return true;
}

} // namespace uddf::samples
