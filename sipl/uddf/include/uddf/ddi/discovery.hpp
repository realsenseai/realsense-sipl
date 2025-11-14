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

#ifndef UDDF_DDI_DISCOVERY_HPP
#define UDDF_DDI_DISCOVERY_HPP

#include "uddf/ddi/IDriver.hpp"
#include "uddf/ddi/uuid.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace uddf::ddi {

/**
 * @brief Contains descriptive information about a specific driver implementation.
 *
 * This structure holds metadata about a driver, such as its name, vendor, and
 * the unique type ID of the IDriver interface it implements. This information is
 * used by UDDF to identify and select appropriate drivers.
 */
struct DriverInfo {
    /**
     * @brief The human-readable name of the driver.
     *        Example: "ModelNumberABC123"
     */
    std::string name;
    /**
     * @brief A short description of the driver's functionality or purpose.
     *        Example: "Provides access to virtual camera sensors for testing"
     */
    std::string description;
    /**
     * @brief The vendor or author of the driver.
     *        Example: "NVIDIA Corporation"
     */
    std::string vendor;
    /**
     * @brief The version or revision string of the driver implementation.
     *        Example: "1.2.3"
     */
    std::string revision;
    /**
     * @brief The unique identifier (@ref UUID) for the concrete @ref IDriver
     *        derived type this driver represents. This allows UDDF to distinguish
     *        between different kinds of drivers.
     */
    UUID driverTypeId;
};

/**
 * @brief Interface for discovering available driver implementations provided by
 *        a driver library.
 *
 * Driver libraries must implement this interface to allow the UDDF framework
 * to query the drivers they contain. A single instance of a class derived from
 * IDriverEnumerator is returned by the library's entry point, @ref uddf_discover_drivers().
 */
class IDriverEnumerator {
public:
    /**
     * @brief Virtual destructor for the interface.
     */
    virtual ~IDriverEnumerator() = default;

    /**
     * @brief Gets the name of the driver provider or library.
     *
     * This name is only used for book-keeping purposes.
     *
     * @retval std::string_view  The name of the provider.
     */
    virtual std::string_view GetName() const = 0;

    /**
     * @brief Gets the total number of distinct driver types provided by this
     *        enumerator.
     *
     * @retval size_t  The count of available driver types. This value determines
     *                 the valid range for the index parameter in
     *                 @ref GetDriverInfo() and @ref CreateDriver().
     */
    virtual size_t GetDriverCount() const = 0;

    /**
     * @brief Retrieves information about a specific driver type by its index.
     *
     * The returned pointer points to data that remains valid as long as the
     * @ref IDriverEnumerator instance exists. The pointer itself is non-owning.
     *
     * @param[in] index  The zero-based index of the driver type.
     *                   Valid range: `[0 .. GetDriverCount() - 1]`
     *
     * @retval const DriverInfo*  A non-owning pointer to the driver information
     *                            structure for the specified index.
     * @retval nullptr            If the provided @a index is out of range.
     */
    virtual const DriverInfo* GetDriverInfo(size_t index) const = 0;

    /**
     * @brief Creates an instance of a specific driver type identified by its index.
     *
     * This factory method instantiates the driver object. UDDF takes ownership of driver after
     * this call.
     *
     * @param[in] index  The zero-based index of the driver type to create.
     *                   Valid range: `[0 .. GetDriverCount() - 1]`
     *
     * @retval std::unique_ptr<IDriver>  A unique pointer owning the newly created
     *                                   driver instance.
     * @retval nullptr                   If creation fails (e.g., resource
     *                                   constraints, invalid index, internal
     *                                   error).
     */
    virtual std::unique_ptr<IDriver> CreateDriver(size_t index) = 0;
};

/**
 * @brief A unique pointer managing the lifetime of an @ref IDriverEnumerator
 *        instance.
 */
using DriverEnumeratorPtr = std::unique_ptr<IDriverEnumerator>;

} // namespace uddf::ddi

//--------------------------------------------------------------------------------------------------
// Driver Provider Entry Point
//--------------------------------------------------------------------------------------------------
extern "C" {
    /**
     * @brief Entry point function exported by UDDF driver libraries.
     *
     * Each UDDF-compliant shared library (driver provider) must implement and
     * export this C-linkage function. The UDDF framework dynamically loads the
     * library and calls this function to obtain an @ref IDriverEnumerator
     * instance.
     *
     * The framework uses the returned enumerator to discover and instantiate the
     * drivers provided by the library. The framework takes ownership of the
     * returned @ref DriverEnumeratorPtr.
     *
     * @retval uddf::ddi::DriverEnumeratorPtr  A unique pointer owning the driver
     *                                         enumerator instance for this library.
     * @retval nullptr                         If the enumerator cannot be created
     *                                         (e.g., initialization failure).
     */
    uddf::ddi::DriverEnumeratorPtr uddf_discover_drivers();
}

#endif // UDDF_DDI_DISCOVERY_HPP