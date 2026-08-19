/*
 * Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
 *
 * D457Library.cpp — UDDF discovery entry point for the D457 GMSL module driver.
 *
 * Models uddf/libraries/moduleR0SIM623/R0SIM623Library.cpp. The exported
 * uddf_discover_drivers() lets SIPL find this .so in /usr/lib/nvsipl_drv.
 *
 * CRITICAL: DriverInfo.name ("D457") MUST match the `moduleDriverName` field in the
 * camera config JSON (d457_gmsl.json). It is case-sensitive.
 */
#include "uddf/ddi/discovery.hpp"
#include "D457Module.hpp"

#include <memory>
#include <string>

namespace uddf::cdd {

const ddi::DriverInfo g_d457ModuleDriverInfo = {
    .name        = "D457",
    .description = "A GMSL module driver for RealSense D457 (DS5 ASIC + MAX9295) via UDDF",
    .vendor      = "RealSense",
    .revision    = "0.1.0"
};

class D457Enumerator final : public ddi::IDriverEnumerator {
public:
    std::string_view GetName() const override { return "D457CameraModuleDriverProvider"; }

    size_t GetDriverCount() const override { return 1; }

    const ddi::DriverInfo* GetDriverInfo(size_t index) const override {
        return (index == 0) ? &g_d457ModuleDriverInfo : nullptr;
    }

    std::unique_ptr<ddi::IDriver> CreateDriver(size_t index) override {
        return (index == 0) ? std::make_unique<d457::D457Module>() : nullptr;
    }
};

} // namespace uddf::cdd

extern "C" {
    uddf::ddi::DriverEnumeratorPtr uddf_discover_drivers() {
        return std::make_unique<uddf::cdd::D457Enumerator>();
    }
}
