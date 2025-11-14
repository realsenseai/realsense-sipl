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

#include "uddf/ddi/discovery.hpp"
#include "uddf/ddi/DriverTypeIds.hpp"

#include "Vb1940ModuleDriver.hpp"
#include "HsbTransportDriver.hpp"
#include <memory>
#include <vector>
#include <string>

namespace uddf::samples {

// Static driver info for the VB1940 Driver
const ddi::DriverInfo g_vb1940DriverInfo = {
    .name         = "Vb1940",
    .description  = "A driver demonstrating CoE (Camera over Ethernet) functionality via UDDF for the VB1940 camera",
    .vendor       = "NVIDIA Corporation - Sample",
    .revision     = "1.0.0",
    .driverTypeId = uddf::ddi::drivers::COE_MODULE_DRIVER_ID
};

const ddi::DriverInfo g_hsbTransportDriverInfo = {
    .name         = "HsbTransport",
    .description  = "A driver demonstrating HSB (Holoscan Sensor Bridge) functionality via UDDF for the Eagle camera",
    .vendor       = "NVIDIA Corporation - Sample",
    .revision     = "1.0.0",
    .driverTypeId = uddf::ddi::drivers::HSB_TRANSPORT_DRIVER_ID
};

/**
 * @brief Implementation of IDriverEnumerator for the Eagle Library.
 *
 * This class allows the UDDF framework to discover the VB1940 driver
 * provided by this library.
 */
class EagleLibraryEnumerator final : public ddi::IDriverEnumerator {
public:
    /**
     * @brief Gets the name of this driver provider.
     */
    std::string_view GetName() const override {
        return "EagleLibraryProvider";
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
            return &g_vb1940DriverInfo;
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
            // Instantiate the VB1940 driver
            return std::make_unique<Vb1940ModuleDriver>();
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
        return std::make_unique<uddf::samples::EagleLibraryEnumerator>();
    }
}
