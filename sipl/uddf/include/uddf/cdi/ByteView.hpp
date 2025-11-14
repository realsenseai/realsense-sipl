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

#ifndef UDDF_CDI_BYTEVIEW_HPP
#define UDDF_CDI_BYTEVIEW_HPP

#include <cstdint>
#include <vector>
#include <array>
#include <cstddef>

namespace uddf::cdi {

/**
 * @brief A minimal, non-owning view over a contiguous sequence of constant bytes.
 */
class ByteView {
public:
    /**
     * @brief Default constructor, creates an empty view.
     */
    constexpr ByteView()  = default;

    /**
     * @brief Constructs a view from a pointer to const uint8_t and a count.
     *
     * @param[in] ptr    Pointer to the first element.
     *                   Valid value: non-nullptr if count > 0.
     * @param[in] count  Number of elements.
     *                   Valid range: [0 .. SIZE_MAX].
     */
    constexpr ByteView(const uint8_t* ptr, size_t count)  : m_ptr(ptr), m_size(count) {}

    /**
     * @brief Constructs a view from a pointer to mutable uint8_t and a count.
     *
     * @note The view over the bytes will stil be const.
     *
     * @param[in] ptr    Pointer to the first element.
     *                   Valid value: non-nullptr if count > 0.
     * @param[in] count  Number of elements.
     *                   Valid range: [0 .. SIZE_MAX].
     */
    constexpr ByteView(uint8_t* ptr, size_t count)  : m_ptr(ptr), m_size(count) {}

    /**
     * @brief Constructs a ByteView from a std::vector of uint8_t.
     *
     * @param[in] vec  The vector to create a view from.
     */
    ByteView(const std::vector<uint8_t>& vec)
        : m_ptr(vec.data()), m_size(vec.size()) {}

    /**
     * @brief Constructs a ByteView from a std::vector of uint8_t.
     *
     * @param[in] vec  The vector to create a view from.
     */
    ByteView(std::vector<uint8_t>& vec)
        : m_ptr(vec.data()), m_size(vec.size()) {}

    /**
     * @brief Constructs a ByteView from a std::array of uint8_t.
     *
     * @param[in] arr  The array to create a view from.
     */
    template <std::size_t N>
    constexpr ByteView(const std::array<uint8_t, N>& arr)
        : m_ptr(arr.data()), m_size(N) {}

    /**
     * @brief Constructs a ByteView from a std::array of uint8_t.
     *
     * @param[in] arr  The array to create a view from.
     */
    template <std::size_t N>
    constexpr ByteView(std::array<uint8_t, N>& arr)
        : m_ptr(arr.data()), m_size(N) {}

    /**
     * @brief Returns a pointer to the beginning of the sequence.
     *
     * @retval const uint8_t*  Pointer to the first element, or nullptr if the view
     *                         is empty.
     */
    constexpr const uint8_t* data() const  { return m_ptr; }

    /**
     * @brief Returns the number of elements in the sequence.
     *
     * @retval size_t  The number of elements.
     */
    constexpr size_t size() const  { return m_size; }

    /**
     * @brief Checks if the view is empty.
     *
     * @retval true   If the view contains no elements.
     * @retval false  Otherwise.
     */
    constexpr bool empty() const  { return m_size == 0; }

private:
    /** Pointer to the first element. */
    const uint8_t* m_ptr = nullptr;
    /** Number of elements in the sequence. */
    size_t m_size = 0;
};

} // namespace uddf::cdi

#endif // UDDF_CDI_BYTEVIEW_HPP
