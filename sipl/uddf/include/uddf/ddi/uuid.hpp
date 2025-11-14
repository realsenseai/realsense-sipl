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

#ifndef UDDF_DDI_UUID_HPP
#define UDDF_DDI_UUID_HPP

#include <cstdint>
#include <array>
#include <tuple>
#include <string>
#include <cstdio>
#include <sstream>
#include <iomanip>

namespace uddf::ddi {

/**
 * @brief Represents a 128-bit Universally Unique Identifier (UUID).
 *
 * This class provides a standard structure for storing a UUID as defined by
 * RFC 4122/RFC 9562. UUIDs are used to uniquely identify information in
 * computer systems. The structure consists of time-based fields, a clock
 * sequence, and a node identifier.
 *
 * The structure includes fields corresponding to the standard UUID layout:
 * - `time_low`: Low 32 bits of the timestamp.
 * - `time_mid`: Middle 16 bits of the timestamp.
 * - `time_hi_and_version`: High 16 bits of the timestamp multiplexed with the
 *   version.
 * - `clock_seq`: 16-bit clock sequence field (may combine sequence, reserved
 *   bits, and variant depending on UUID version/implementation).
 * - `node`: 48-bit node identifier.
 *
 * @see https://en.wikipedia.org/wiki/Universally_unique_identifier
 * @see https://www.rfc-editor.org/rfc/rfc4122
 * @see https://www.rfc-editor.org/rfc/rfc9562
 */
class UUID
{
public:
    /**
     * @brief Low 32 bits of the timestamp.
     */
    uint32_t time_low{0U};
    /**
     * @brief Middle 16 bits of the timestamp.
     */
    uint16_t time_mid{0U};
    /**
     * @brief High 16 bits of the timestamp multiplexed with the version.
     */
    uint16_t time_hi_and_version{0U};
    /**
     * @brief 16-bit clock sequence field (encodes sequence, variant, reserved).
     */
    uint16_t clock_seq{0U};
    /**
     * @brief 48-bit node identifier.
     */
    std::array<uint8_t, 6> node{};

    /**
     * @brief Constructs a UUID from individual field values.
     *
     * Initializes the UUID according to the standard component layout.
     *
     * @param[in] low   Low 32 bits of the timestamp. Valid range: [0 .. UINT32_MAX]
     * @param[in] mid   Middle 16 bits of the timestamp. Valid range: [0 .. UINT16_MAX]
     * @param[in] high  High 16 bits of the timestamp and version. Valid range: [0 .. UINT16_MAX]
     * @param[in] seq   16-bit clock sequence field. Valid range: [0 .. UINT16_MAX]
     * @param[in] n0    First byte of the 48-bit node identifier. Valid range: [0 .. 255]
     * @param[in] n1    Second byte of the node identifier. Valid range: [0 .. 255]
     * @param[in] n2    Third byte of the node identifier. Valid range: [0 .. 255]
     * @param[in] n3    Fourth byte of the node identifier. Valid range: [0 .. 255]
     * @param[in] n4    Fifth byte of the node identifier. Valid range: [0 .. 255]
     * @param[in] n5    Sixth byte of the node identifier. Valid range: [0 .. 255]
     */
    constexpr UUID(uint32_t const low, uint16_t const mid,
         uint16_t const high, uint16_t const seq,
         uint8_t const n0, uint8_t const n1, uint8_t const n2,
         uint8_t const n3, uint8_t const n4, uint8_t const n5) noexcept
        : time_low(low)
        , time_mid(mid)
        , time_hi_and_version(high)
        , clock_seq(seq)
        , node{{n0,n1,n2,n3,n4,n5}}
    { }

    /**
     * @brief Default constructor. Initializes to the Nil UUID (all zeros).
     *        See RFC 4122, Section 4.1.7.
     */
    constexpr UUID() noexcept = default;

    /**
     * @brief Compares this UUID with another for equality.
     *
     * Two UUIDs are equal if and only if all their corresponding fields are
     * identical.
     *
     * @param[in] other  The UUID to compare against.
     *
     * @retval true   The UUIDs are identical.
     * @retval false  The UUIDs differ in at least one field.
     */
    constexpr bool operator==(const UUID &other) const noexcept
    {
        // std::tie compares all members lexicographically
        return std::tie(time_low, time_mid, time_hi_and_version, clock_seq, node) ==
               std::tie(other.time_low, other.time_mid, other.time_hi_and_version, other.clock_seq, other.node);
    }

    /**
     * @brief Compares this UUID with another for inequality.
     *
     * Equivalent to `!(*this == other)`.
     *
     * @param[in] other  The UUID to compare against.
     *
     * @retval true   The UUIDs differ in at least one field.
     * @retval false  The UUIDs are identical.
     */
    constexpr bool operator!=(const UUID &other) const noexcept
    {
        return !(*this == other);
    }

    /**
     * @brief Formats the UUID into its canonical string representation.
     *
     * Returns the UUID as a string in the format
     * `xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx`, where 'x' represents a
     * hexadecimal digit.
     *
     * @return std::string The formatted UUID string.
     */
    std::string toString() const
    {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');

        oss << std::setw(8) << time_low << '-';
        oss << std::setw(4) << time_mid << '-';
        oss << std::setw(4) << time_hi_and_version << '-';
        // Print clock_seq as two separate bytes for standard formatting
        oss << std::setw(2) << static_cast<uint16_t>(clock_seq >> 8); // High byte
        oss << std::setw(2) << static_cast<uint16_t>(clock_seq & 0xFF) << '-'; // Low byte
        for (size_t i = 0; i < node.size(); ++i) {
             oss << std::setw(2) << static_cast<uint16_t>(node[i]);
        }

        return oss.str();
    }
};

static_assert(sizeof(UUID) == 16, // Standard UUID size is 128 bits (16 bytes)
              "UUID structure size mismatch - expected 16 bytes.");

/**
 * @brief Contains UUID-related constants and utilities.
 */
namespace uuid {

/**
 * @brief The Nil UUID (all bits set to zero).
 *
 * Defined in RFC 4122, Section 4.1.7. Represents an invalid or uninitialized UUID.
 * Its canonical textual representation is `00000000-0000-0000-0000-000000000000`.
 *
 */
inline constexpr UUID UUID_NIL = {};

} // namespace uuid

} // namespace uddf::ddi

#endif //UDDF_DDI_UUID_HPP
