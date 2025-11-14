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

#ifndef UDDF_SAMPLES_D555EXTENDEDINTERFACE_HPP
#define UDDF_SAMPLES_D555EXTENDEDINTERFACE_HPP

#include "uddf/ddi/uuid.hpp"
#include "uddf/ddi/IInterface.hpp"

#include <string>
#include <cstdint>
#include <vector>
#include <netinet/in.h>
#include <linux/if_ether.h>
#include <linux/if.h>

namespace uuid {

/**
 * @brief The unique identifier for the @ref IReadWriteI2C interface type.
 */
inline constexpr uddf::ddi::UUID D555_EXTENDED_INTERFACE_ID = uddf::ddi::UUID( 0x9f3e7c1a, 0x4b8d, 0x4265, 0xc8f1, 0x5e, 0x92, 0x6b, 0x3f, 0x20, 0x19 );

} // namespace uuid

class D555ExtendedInterface : public uddf::ddi::IInterface {
public:
    D555ExtendedInterface() {};
    ~D555ExtendedInterface() {}

    static constexpr uddf::ddi::UUID id { uuid::D555_EXTENDED_INTERFACE_ID };

    virtual bool setStreamId(const uint8_t stream_id) = 0;
};

#endif // UDDF_SAMPLES_D555EXTENDEDINTERFACE_HPP
