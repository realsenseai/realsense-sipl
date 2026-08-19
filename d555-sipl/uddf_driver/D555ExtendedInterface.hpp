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
