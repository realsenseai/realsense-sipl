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

#ifndef UDDF_SAMPLES_COEMODULEDRIVERBASE_HPP
#define UDDF_SAMPLES_COEMODULEDRIVERBASE_HPP

#include "uddf/ddi/IDriver.hpp"
#include "uddf/ddi/interfaces/ICameraModule.hpp"
#include "uddf/ddi/interfaces/ICoEModuleControl.hpp"
#include "uddf/ddi/uuid.hpp"

namespace uddf::samples {

using namespace uddf::ddi::interfaces;

/**
 * @brief Base definition for Camera over Ethernet (CoE) module drivers (Sample version).
 *
 * This class represents a driver instance capable of controlling a CoE module.
 */
class CoEModuleDriverBase : public uddf::ddi::IDriver,
                            public ICameraModule,
                            public ICoEModuleControl {
public:

    /**
     * @brief Retrieves a pointer to a specific functional interface supported by this driver.
     *
     * @param[in] interface_uuid The @ref UUID of the desired interface type.
     *
     * @retval IInterface* A non-owning pointer to the requested interface if supported.
     * @retval nullptr If the requested interface is not supported directly by this class.
     */
    uddf::ddi::IInterface* GetInterface(const uddf::ddi::UUID& interface_uuid) noexcept override {
        if (interface_uuid == ICameraModule::id) {
            return static_cast<ICameraModule*>(this);
        }
        if (interface_uuid == ICoEModuleControl::id) {
            return static_cast<ICoEModuleControl*>(this);
        }
        return GetExtendedInterface(interface_uuid);
    }

    /**
     * @brief Virtual destructor for the CoE module driver base class.
     */
    ~CoEModuleDriverBase() override = default;

protected:

    /**
     * @brief Extension point for retrieving interfaces not directly aggregated by CoEModuleDriverBase.
     *
     * A subclass can override this implementation to support any additional interfaces
     * that driver might support.
     *
     * If this driver implemented other interfaces (e.g., ISomeOtherControl),
     * you would check for their UUIDs here:
     * if (uuid == ISomeOtherControl::id) {
     *     return static_cast<ISomeOtherControl*>(this);
     * }
     *
     * @param[in] uuid The UUID of the requested interface.
     * @return IInterface* Pointer to the interface if supported, nullptr otherwise.
     */
    virtual uddf::ddi::IInterface* GetExtendedInterface(const uddf::ddi::UUID& uuid) noexcept {
        return nullptr;
    }
};

} // namespace uddf::samples

#endif // UDDF_SAMPLES_COEMODULEDRIVERBASE_HPP
