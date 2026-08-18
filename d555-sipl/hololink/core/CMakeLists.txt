# SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

add_library(hololink_core_realsense STATIC
  arp_wrapper.cpp
  data_channel.cpp
  enumerator.cpp
  hololink.cpp
  jesd.cpp
  logging.cpp
  metadata.cpp
  networking.cpp
  timeout.cpp
  tools.cpp
  traditional_spi.cpp
  traditional_i2c.cpp
  )
add_library(hololink::core ALIAS hololink_core_realsense)

set_property(TARGET hololink_core_realsense PROPERTY POSITION_INDEPENDENT_CODE ON)

# fmt: prefer an installed copy; fall back to the pinned tree in ../../fmt.
# The vendored copy is fmt 10.2.1, kept so this driver builds on a Jetson that has no fmt package.
# It is header-only here, so the fallback costs an include path and nothing else.
find_package(fmt QUIET)
if(fmt_FOUND)
  message(STATUS "hololink_core_realsense: using the installed fmt (${fmt_VERSION})")
  target_link_libraries(hololink_core_realsense PUBLIC fmt::fmt-header-only)
else()
  message(STATUS "hololink_core_realsense: no fmt package found, using the vendored copy in fmt/")
  target_compile_definitions(hololink_core_realsense PUBLIC FMT_HEADER_ONLY)
  target_include_directories(hololink_core_realsense
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../fmt
    INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../fmt>)
endif()

target_include_directories(hololink_core_realsense
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>
    $<INSTALL_INTERFACE:src>
  )

# Installation of the hololink_core_realsense library
install(TARGETS hololink_core_realsense
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT hololink-core)

