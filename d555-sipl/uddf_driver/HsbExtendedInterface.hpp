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

#ifndef UDDF_SAMPLES_HSBEXTENDEDINTERFACE_HPP
#define UDDF_SAMPLES_HSBEXTENDEDINTERFACE_HPP

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
inline constexpr uddf::ddi::UUID HSB_EXTENDED_INTERFACE_ID = uddf::ddi::UUID( 0x9f3e7c1a, 0x4b8d, 0x4265, 0xc8f1, 0x5e, 0x92, 0x6b, 0x3f, 0x74, 0x18 );

} // namespace uuid

/* Structure to hold discovered Hololink device information */
struct hsb_info {
    uint8_t mac[ETH_ALEN];
    char ip_address[INET_ADDRSTRLEN]; /* Store as string */
    uint16_t board_id;
    uint8_t serial_number[8];  /* 7 bytes raw data, not null-terminated */
    uint16_t control_port;
    /* Interface information */
    uint32_t interface_index;
    char interface_name[IFNAMSIZ + 1];
    char interface_address[INET_ADDRSTRLEN];
    char destination_address[INET_ADDRSTRLEN];
    /* Firmware versions */
    uint16_t cpnx_version;
    uint16_t cpnx_crc;
    uint16_t clnx_version;
    uint16_t clnx_crc;
    /* Add other fields if needed (e.g., data_plane, transaction_id) */
};

class HsbExtendedInterface : public uddf::ddi::IInterface {
public:
    HsbExtendedInterface() {};
    ~HsbExtendedInterface() {}

    static constexpr uddf::ddi::UUID id { uuid::HSB_EXTENDED_INTERFACE_ID };

    virtual std::vector<hsb_info> EnumerateDevices(uint32_t timeout_seconds, const char *target_interface) = 0;
};

#endif // UDDF_SAMPLES_HSBEXTENDEDINTERFACE_HPP
