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
