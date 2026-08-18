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

#ifndef UDDF_SAMPLES_D555SENSORDRIVER_HPP
#define UDDF_SAMPLES_D555SENSORDRIVER_HPP

#include "uddf/ddi/interfaces/ISensorControl.hpp"

namespace uddf::samples {

using namespace uddf::ddi::interfaces; // Using namespace directive

/**
 * @brief The declaration of the D555SensorDriver class.
 *
 * This object implements ISensorControl for the D555 camera module used in Realsense.
 */
class D555SensorDriver final : public ISensorControl {
public:
    /**
     * @brief Construct a new D555SensorDriver object
     */
    explicit D555SensorDriver();

    /**
     * @brief Destroy the D555SensorDriver object
     */
    ~D555SensorDriver() override;

    // --- ISensorControl Methods ---

    /**
     * @brief Gets the sensor attributes (stub implementation).
     * @param[in] cdi CDI interface reference.
     * @param[out] attributes Output structure for attributes.
     * @return Always returns true in this stub.
     */
    bool GetSensorAttributes(uddf::cdi::IHardwareAccess& hwAccess, SensorAttributes& attributes) const override;

    /**
     * @brief Sets the sensor controls (stub implementation).
     * @param[in] cdi CDI interface reference.
     * @param[in] controls Input structure with desired controls.
     * @return Always returns true in this stub.
     */
    bool SetSensorControls(uddf::cdi::IHardwareAccess& hwAccess, const SensorControls& controls) override;

    /**
     * @brief Parses top embedded data (stub implementation).
     * @param[in] cdi CDI interface reference.
     * @param[in] chunk Input data chunk.
     * @param[out] info Output structure for parsed info.
     * @return Always returns true in this stub.
     */
    bool ParseTopEmbeddedData(uddf::cdi::IHardwareAccess& hwAccess, const EmbeddedDataChunk& chunk, EmbeddedDataInfo& info) override;

    /**
     * @brief Parses bottom embedded data (stub implementation).
     * @param[in] cdi CDI interface reference.
     * @param[in] chunk Input data chunk.
     * @param[out] info Output structure for parsed info.
     * @return Always returns true in this stub.
     */
    bool ParseBottomEmbeddedData(uddf::cdi::IHardwareAccess& hwAccess, const EmbeddedDataChunk& chunk, EmbeddedDataInfo& info) override;
};

} // namespace uddf::samples

#endif // UDDF_SAMPLES_D555SENSORDRIVER_HPP
