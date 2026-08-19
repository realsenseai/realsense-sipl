/*
 * SPDX-FileCopyrightText: Copyright (c) 2025-2026 RealSense AI. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
