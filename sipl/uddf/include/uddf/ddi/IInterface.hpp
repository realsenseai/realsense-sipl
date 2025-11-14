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

#ifndef UDDF_DDI_IINTERFACE_HPP
#define UDDF_DDI_IINTERFACE_HPP

#include "uddf/ddi/uuid.hpp"

namespace uddf::ddi {

/**
 * @brief Base interface for all functional interfaces provided by UDDF drivers.
 *
 * This is the fundamental building block for all specific capabilities offered
 * by a driver (e.g., sensor control, camera module control). All concrete
 * functional interfaces must derive from @ref IInterface.
 *
 * Instances of classes derived from @ref IInterface are obtained via
 * @ref IDriver::GetInterface() and are non-owning views into the driver's
 * functionality. Their lifetime is tied to the @ref IDriver object that
 * provided them.
 */
class IInterface {
public:
    /**
     * @brief Static UUID representing the base interface type.
     */
    static constexpr UUID id { uuid::UUID_NIL };

protected:
    /**
     * @brief Protected constructor.
     */
    IInterface() = default;

    /**
     * @brief Protected virtual destructor.
     */
    virtual ~IInterface() = default;

private:

    /* Make all interfaces non-copyable and non-movable */
    IInterface(IInterface const&) = delete;
    IInterface& operator=(IInterface const&) = delete;
    IInterface(IInterface&&) = delete;
    IInterface& operator=(IInterface&&) = delete;
};

} // namespace uddf::ddi

#endif // UDDF_DDI_IINTERFACE_HPP
