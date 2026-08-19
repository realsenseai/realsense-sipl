/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include <hololink/core/logging_internal.hpp>

template <>
struct fmt::formatter<NvSciBufAttrValColorFmt> : fmt::formatter<fmt::string_view> {
    auto format(const NvSciBufAttrValColorFmt& nvsci_fmt, fmt::format_context& ctx) const -> decltype(ctx.out())
    {
#define FMT(f)           \
    case NvSciColor_##f: \
        return fmt::format_to(ctx.out(), "NvSciColor_" #f);
        switch (nvsci_fmt) {
            FMT(U8V8)
            FMT(U8_V8)
            FMT(V8U8)
            FMT(U10V10)
            FMT(V10U10)
            FMT(U12V12)
            FMT(V12U12)
            FMT(U16V16)
            FMT(V16U16)
            FMT(Y8)
            FMT(Y10)
            FMT(Y12)
            FMT(Y16)
            FMT(U8)
            FMT(U10)
            FMT(U12)
            FMT(U16)
            FMT(V8)
            FMT(V10)
            FMT(V12)
            FMT(V16)
            FMT(X2Rc10Rb10Ra10_Bayer10RGGB)
            FMT(X2Rc10Rb10Ra10_Bayer10BGGR)
            FMT(X2Rc10Rb10Ra10_Bayer10GRBG)
            FMT(X2Rc10Rb10Ra10_Bayer10GBRG)
            // RAW16 (DT 0x2E) formats the D457 delivers for depth/RGB/IR:
            FMT(Bayer16RGGB)
            FMT(Bayer16BGGR)
            FMT(Bayer16GRBG)
            FMT(Bayer16GBRG)
        default:
            throw std::runtime_error(fmt::format("Unknown format ({})", static_cast<int>(nvsci_fmt)));
        }
    }
};
