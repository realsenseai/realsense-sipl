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

#ifndef UDDF_DDI_IDRIVER_HPP
#define UDDF_DDI_IDRIVER_HPP

#include "uddf/ddi/IInterface.hpp"
#include "uddf/ddi/uuid.hpp"

namespace uddf::ddi {

/**
 * @brief Base interface for all UDDF driver objects.
 *
 * This is the core interface that all specific driver implementations (e.g.,
 * camera sensor drivers, CoE module drivers) must inherit from. It represents
 * an instantiated driver object capable of providing various functional
 * interfaces.
 */
class IDriver {
public:
    /**
     * @brief Virtual destructor for the driver interface.
     */
    virtual ~IDriver() = default;

    /**
     * @brief Gets the unique identifier for the concrete driver type.
     *
     * @return The unique identifier of this concrete driver type.
     */
    virtual UUID GetID() const noexcept = 0;

    /**
     * @brief Retrieves a pointer to a specific functional interface supported by this driver.
     *
     * Drivers implement various functionalities through interfaces derived from
     * @ref IInterface. UDDF will query for these interfaces using their unique interface UUIDs.
     *
     * The lifetime of the returned @ref IInterface pointer is bound to the
     * lifetime of the @ref IDriver object that provided it. The pointer is
     * non-owning; the @ref IDriver instance retains ownership of the underlying
     * interface implementation.
     *
     * @param[in] uuid  The @ref UUID of the desired interface type
     *                  Valid value: A valid interface @ref UUID.
     *
     * @retval IInterface*  A non-owning pointer to the requested interface if
     *                      supported by this driver.
     * @retval nullptr      If the driver does not support the requested interface
     *                      identified by @a interface_uuid.
     */
    virtual IInterface* GetInterface(const UUID& uuid) noexcept = 0;
};

} // namespace uddf::ddi

namespace {

template <typename TargetInterface>
TargetInterface* interface_cast(uddf::ddi::IDriver* driver) noexcept {
    static_assert(std::is_base_of_v<uddf::ddi::IInterface, TargetInterface>,
                  "TargetInterface must derive from uddf::ddi::IInterface");
    if (!driver) {
        return nullptr;
    }

    uddf::ddi::IInterface* base_iface = driver->GetInterface(TargetInterface::id);

    if (!base_iface) {
        return nullptr;
    }

    return static_cast<TargetInterface*>(base_iface);
}

} // anonymous namespace

#endif // UDDF_DDI_IDRIVER_HPP
