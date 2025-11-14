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

#ifndef UDDF_CDI_HAL_HSLRESULT_HPP
#define UDDF_CDI_HAL_HSLRESULT_HPP

#include <cstdint>
#include <variant>
#include <string_view>

namespace uddf::cdi {

/**
 * @brief Represents a successful HSL operation.
 */
struct HSLResultSuccess {
    // Success operations currently require no additional data
};

/**
 * @brief Represents an I2C error during an HSL operation.
 */
struct HSLResultI2CError {

    enum I2CErrorType : uint8_t {
        HSL_I2C_ERROR_NONE         = 0,
        HSL_I2C_ERROR_NACK         = 1,
        HSL_I2C_ERROR_BUS_TIMEOUT  = 2,
        HSL_I2C_ERROR_ARBLOST      = 3,     ///< Arbitration lost
        HSL_I2C_ERROR_READ_VERIFY  = 4,     ///< HSL readVerify operation failed
        HSL_I2C_ERROR_POLL_TIMEOUT = 5,     ///< Timeout on HSL poll operation
        HSL_I2C_ERROR_READBACK     = 6,     ///< Readback verification failed
        HSL_I2C_ERROR_UNKNOWN      = 0xFE,  ///< Unknown I2C error
    };

    I2CErrorType type   {HSL_I2C_ERROR_NONE};

    uint16_t address    {0U};   ///< I2C address of the device that reported the error
    uint16_t offset     {0U};   ///< Read/write offset in the statement that caused the error
    uint16_t value      {0U};   ///< Value read from or written to the device
    uint16_t expected   {0U};   ///< Expected value (if applicable)
    uint16_t opIndex    {0U};   ///< HSL blob index of the operation that caused the error (optional)

    std::string_view typeName() {
        std::string_view ret {};
        switch (type) {
            case I2CErrorType::HSL_I2C_ERROR_NONE:
                ret = std::string_view{"HSL_I2C_ERROR_NONE"};
                break;
            case I2CErrorType::HSL_I2C_ERROR_NACK:
                ret = std::string_view{"HSL_I2C_ERROR_NACK"};
                break;
            case I2CErrorType::HSL_I2C_ERROR_BUS_TIMEOUT:
                ret = std::string_view{"HSL_I2C_ERROR_BUS_TIMEOUT"};
                break;
            case I2CErrorType::HSL_I2C_ERROR_ARBLOST:
                ret = std::string_view{"HSL_I2C_ERROR_ARBLOST"};
                break;
            case I2CErrorType::HSL_I2C_ERROR_READ_VERIFY:
                ret = std::string_view{"HSL_I2C_ERROR_READ_VERIFY"};
                break;
            case I2CErrorType::HSL_I2C_ERROR_POLL_TIMEOUT:
                ret = std::string_view{"HSL_I2C_ERROR_POLL_TIMEOUT"};
                break;
            case I2CErrorType::HSL_I2C_ERROR_READBACK:
                ret = std::string_view{"HSL_I2C_ERROR_READBACK"};
                break;
            default:
                ret = std::string_view{"HSL_I2C_ERROR_UNKNOWN"};
                break;
        }
        return ret;
    }
};

/**
 * @brief Represents a framework error during an HSL operation.
 */
struct HSLResultFrameworkError {

    enum FrameworkErrorType : uint8_t {
        HSL_FRAMEWORK_ERROR_NONE         = 0,
        HSL_FRAMEWORK_ERROR_DELIVERY     = 1,   ///< Failed to deliver HSL blob to the processing engine
        HSL_FRAMEWORK_ERROR_TIMEOUT      = 2,   ///< Timeout waiting for HSL engine to process the blob
        HSL_FRAMEWORK_ERROR_INVALID_BLOB = 3,   ///< Invalid HSL blob (e.g. wrong version, illegal opcode, etc.)
        HSL_FRAMEWORK_ERROR_UNSUPPORTED  = 4,   ///< Unsupported HSL operation
        HSL_FRAMEWORK_ERROR_PREVIOUS     = 5,   ///< Previous error occurred; no more processing will be done
    };

    FrameworkErrorType type {HSL_FRAMEWORK_ERROR_NONE};
    uint8_t badOpcode       {0U};      ///< Opcode that caused the UNSUPPORTED or (possibly) INVALID_BLOB error

    HSLResultFrameworkError() : type(HSL_FRAMEWORK_ERROR_NONE), badOpcode(0U) {}
    HSLResultFrameworkError(FrameworkErrorType type, uint8_t badOpcode=0U) : type(type), badOpcode(badOpcode) {}
};

/**
 * @brief Represents the result of an HSL operation.
 *
 * An HSLResult describes the result of processing an HSL blob.  It can be cast to
 * a bool to check if the operation was successful.  If the operation was not
 * successful, the result field will hold a specific error type describing the
 * failure.
 */
struct HSLResult {

    std::variant<HSLResultSuccess, HSLResultI2CError, HSLResultFrameworkError> result;

    operator bool() const {
        return std::holds_alternative<HSLResultSuccess>(result);
    }

    HSLResult() : result(HSLResultSuccess()) {}
    HSLResult(HSLResultI2CError const& i2cError) : result(i2cError) {}
    HSLResult(HSLResultFrameworkError const& frameworkError) : result(frameworkError) {}

    HSLResultI2CError const* asI2CError() const {
        return std::get_if<HSLResultI2CError>(&result);
    }

    HSLResultFrameworkError const* asFrameworkError() const {
        return std::get_if<HSLResultFrameworkError>(&result);
    }
};

} // namespace uddf::cdi

#endif // UDDF_CDI_HAL_HSLRESULT_HPP
