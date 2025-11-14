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

#ifndef UDDF_CDI_IDRIVERSERVICES_HPP
#define UDDF_CDI_IDRIVERSERVICES_HPP

#include <cstdint>

namespace uddf::cdi {

/**
 * @brief Interface providing basic services to drivers.
 */
class IDriverServices {
public:

    enum class LogLevel {
        DBUG,
        INFO,
        WARNING,
        ERROR,
        FATAL
    };

    /**
     * @brief Log a message.
     *
     * @param[in] level The log level.
     * @param[in] file The name of the source file originating the message.
     * @param[in] line The line number in the source file.
     * @param[in] message The message to log.
     */
    virtual void Log(LogLevel level, const char* file, uint32_t line, const char* message) = 0;

    enum class ErrorLevel {
        WARNING,
        ERROR,
        FATAL
    };

    /**
     * @brief Report an error.
     *
     * @param[in] level The error level.
     * @param[in] message The message to report.
     */
    virtual void ReportError(ErrorLevel level, const char* message) = 0;

protected:

    virtual ~IDriverServices() = default;
};

} // namespace uddf::cdi

#endif // UDDF_CDI_IDRIVERSERVICES_HPP
