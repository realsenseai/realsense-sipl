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

#include "SampleCoEDriver.hpp"
#include "SampleHsbDriver.hpp"
#include <memory>
#include <vector>
#include <string>

namespace uddf::samples {

// Static driver info for the Sample CoE Driver
const ddi::DriverInfo g_sampleCoEDriverInfo = {
    .name         = "SampleCoEDriver",
    .description  = "A sample driver demonstrating CoE (Camera over Ethernet) functionality via UDDF",
    .vendor       = "NVIDIA Corporation - Sample",
    .revision     = "1.0.0",
    .driverTypeId = uddf::ddi::drivers::COE_MODULE_DRIVER_ID // Use the ID from the base class.
};

// Static driver info for the *Variant* Sample CoE Driver
const ddi::DriverInfo g_sampleCoEDriverInfo_Variant = {
    .name         = "SampleCoEDriverVariant", // Different name
    .description  = "A *variant* sample driver demonstrating CoE via UDDF", // Slightly different description
    .vendor       = "NVIDIA Corporation - Sample",
    .revision     = "1.0.1", // Different revision
    .driverTypeId = uddf::ddi::drivers::COE_MODULE_DRIVER_ID // Same interface ID.
};

const ddi::DriverInfo g_sampleHsbDriverInfo = {
    .name         = "SampleHsbDriver",
    .description  = "A sample driver demonstrating Holoscan Sensor Bridge functionality via UDDF",
    .vendor       = "NVIDIA Corporation - Sample",
    .revision     = "1.0.0",
    .driverTypeId = uddf::ddi::drivers::HSB_TRANSPORT_DRIVER_ID // Use the ID from the base class.
};

/**
 * @brief Implementation of IDriverEnumerator for the Sample CoE Library.
 *
 * This class allows the UDDF framework to discover the SampleCoEDriver
 * provided by this library.
 */
class SampleCoELibraryEnumerator final : public ddi::IDriverEnumerator {
public:
    /**
     * @brief Gets the name of this driver provider.
     */
    std::string_view GetName() const override {
        return "SampleCoELibraryProvider";
    }

    /**
     * @brief Gets the number of driver types provided (two in this case).
     */
    size_t GetDriverCount() const override {
        return 3; // We provide the standard and variant SampleCoEDriver
    }

    /**
     * @brief Retrieves information about the driver at the given index.
     */
    const ddi::DriverInfo* GetDriverInfo(size_t index) const override {
        switch (index) {
        case 0:
            return &g_sampleCoEDriverInfo;
        case 1:
            return &g_sampleCoEDriverInfo_Variant;
        case 2:
            return &g_sampleHsbDriverInfo;
        default:
            return nullptr; // Index out of bounds
        }
    }

    /**
     * @brief Creates an instance of the driver at the given index.
     */
    std::unique_ptr<ddi::IDriver> CreateDriver(size_t index) override {
        switch (index) {
        case 0:
            // Instantiate the standard driver
            return std::make_unique<SampleCoEDriver>("StandardCoE");
        case 1:
            // Instantiate the variant driver with a different name
            return std::make_unique<SampleCoEDriver>("VariantCoE_ConfigX");
        case 2:
            return std::make_unique<SampleHsbDriver>();
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
        return std::make_unique<uddf::samples::SampleCoELibraryEnumerator>();
    }
}
