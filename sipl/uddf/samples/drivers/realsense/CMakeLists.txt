# SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: LicenseRef-NvidiaProprietary
#
# NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
# property and proprietary rights in and to this material, related
# documentation and any modifications thereto. Any use, reproduction,
# disclosure or distribution of this material and related documentation
# without an express license agreement from NVIDIA CORPORATION or
# its affiliates is strictly prohibited.

cmake_minimum_required(VERSION 3.15 FATAL_ERROR)

project(
  pdk_sample_realsense_driver
  VERSION 1.0.0
  DESCRIPTION "PDK Sample Realsense All-in-One Driver for UDDF"
  LANGUAGES CXX
)

add_subdirectory(hololink/core)

# Set the HSL and UDDF directories relative to this CMakeLists.txt
set(PDK_HSL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../hsl")
set(PDK_UDDF_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../uddf")

set(HSL_PYTHON_SCRIPT_DIR "${PDK_HSL_DIR}/pyhsl")
set(HSL_PYTHON_MODULE_DIR "${PDK_HSL_DIR}/pyhsl")

list(APPEND CMAKE_MODULE_PATH "${PDK_HSL_DIR}/integrations/cmake")
include(hsl_compile_rules)

# Create a shared library for the sample CoE driver
add_library(nvuddf_realsense_library SHARED
    RealsenseLibrary.cpp
    HsbTransportDriver.cpp
    D555ModuleDriver.cpp
    D555SensorDriver.cpp
)

target_compile_features(nvuddf_realsense_library PRIVATE cxx_std_17)

target_include_directories(nvuddf_realsense_library PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    # ${CMAKE_CURRENT_SOURCE_DIR}/hololink/core
    ${CMAKE_CURRENT_SOURCE_DIR}/../realsense/hololink/core
    ${PDK_HSL_DIR}/include
    ${PDK_UDDF_DIR}/include
    ${PDK_UDDF_DIR}/samples/drivers/coe
)

target_link_libraries(nvuddf_realsense_library PRIVATE
    ${PDK_HSL_DIR}/lib-target/libnvhslencoder.so
    hololink_core_realsense
)

hsl_add_driver_sources(nvuddf_realsense_library
  SOURCES d555_hsl.py
  NAMESPACE uddf::samples::d555::hsl
  DEPENDENCIES d555_data.py
)

# Print configuration information
message(STATUS "NV Realsense All-in-one Driver Configuration:")
message(STATUS "  HSL Directory: ${PDK_HSL_DIR}")
message(STATUS "  UDDF Include Directory: ${PDK_UDDF_DIR}")
message(STATUS "  Output Library: libnvuddf_realsense_library.so")
