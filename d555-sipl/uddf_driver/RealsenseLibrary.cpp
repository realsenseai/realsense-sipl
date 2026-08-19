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

#include "uddf/ddi/discovery.hpp"
#include "uddf/ddi/DriverTypeIds.hpp"

#include "D555ModuleDriver.hpp"
#include "HsbTransportDriver.hpp"
#include <memory>
#include <vector>
#include <string>

namespace uddf::samples {

// Static driver info for the D555 Driver
const ddi::DriverInfo g_D555DriverInfo = {
    .name         = "D555",
    .description  = "A driver demonstrating CoE (Camera over Ethernet) functionality via UDDF for the D555 camera",
    .vendor       = "NVIDIA Corporation - Sample",
    .revision     = "1.0.0",
    .driverTypeId = uddf::ddi::drivers::COE_MODULE_DRIVER_ID
};

const ddi::DriverInfo g_hsbTransportDriverInfo = {
    .name         = "HsbTransport",
    .description  = "A driver demonstrating HSB (Holoscan Sensor Bridge) functionality via UDDF for the Realsense camera",
    .vendor       = "NVIDIA Corporation - Sample",
    .revision     = "1.0.0",
    .driverTypeId = uddf::ddi::drivers::HSB_TRANSPORT_DRIVER_ID
};

/**
 * @brief Implementation of IDriverEnumerator for the Realsense Library.
 *
 * This class allows the UDDF framework to discover the D555 driver
 * provided by this library.
 */
class RealsenseLibraryEnumerator final : public ddi::IDriverEnumerator {
public:
    /**
     * @brief Gets the name of this driver provider.
     */
    std::string_view GetName() const override {
        return "RealsenseLibraryProvider";
    }

    /**
     * @brief Gets the number of driver types provided (three in this case).
     */
    size_t GetDriverCount() const override {
        return 2;
    }

    /**
     * @brief Retrieves information about the driver at the given index.
     */
    const ddi::DriverInfo* GetDriverInfo(size_t index) const override {
        switch (index) {
        case 0:
            return &g_D555DriverInfo;
        case 1:
            return &g_hsbTransportDriverInfo;
        default:
            return nullptr;
        }
    }

    /**
     * @brief Creates an instance of the driver at the given index.
     */
    std::unique_ptr<ddi::IDriver> CreateDriver(size_t index) override {
        switch (index) {
        case 0:
            // Instantiate the D555 driver
            return std::make_unique<D555ModuleDriver>();
        case 1:
            // Instantiate the HSB Transport driver
            return std::make_unique<HsbTransportDriver>();
        default:
            return nullptr;
        }
    }
};

} // namespace uddf::samples

//--------------------------------------------------------------------------------------------------
// Driver Provider Entry Point
//--------------------------------------------------------------------------------------------------
extern "C" {
    /**
     * @brief UDDF entry point for this shared library.
     *
     * Called by the UDDF framework to discover drivers provided by this library.
     *
     * @retval uddf::ddi::DriverEnumeratorPtr A unique pointer to the enumerator.
     */
    uddf::ddi::DriverEnumeratorPtr uddf_discover_drivers() {
        return std::make_unique<uddf::samples::RealsenseLibraryEnumerator>();
    }
}
