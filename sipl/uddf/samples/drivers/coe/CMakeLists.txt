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
  pdk_sample_coe_driver
  VERSION 1.0.0
  DESCRIPTION "PDK Sample CoE Driver for UDDF"
  LANGUAGES CXX
)

# Set the HSL and UDDF directories relative to this CMakeLists.txt
set(PDK_HSL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../hsl")
set(PDK_UDDF_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../uddf")

set(HSL_PYTHON_SCRIPT_DIR "${PDK_HSL_DIR}/pyhsl")
set(HSL_PYTHON_MODULE_DIR "${PDK_HSL_DIR}/pyhsl")

list(APPEND CMAKE_MODULE_PATH "${PDK_HSL_DIR}/integrations/cmake")
include(hsl_compile_rules)

# Create a shared library for the sample CoE driver
add_library(sample_coe_library SHARED
    SampleCoEDriver.cpp
    SampleHsbDriver.cpp
    SampleCoELibrary.cpp
)

target_compile_features(sample_coe_library PRIVATE cxx_std_17)

target_include_directories(sample_coe_library PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PDK_UDDF_DIR}/include
    ${PDK_HSL_DIR}/include
)

target_link_libraries(sample_coe_library PRIVATE
    ${PDK_HSL_DIR}/lib-target/libnvhslencoder.so
)

hsl_add_driver_sources(sample_coe_library
  SOURCES Sequence_Lifecycle.py
  CONFIG_PARAMS HSL_EXAMPLE_DEFINE=1234
  NAMESPACE uddf::samples::coe::hsl
)

# Print configuration information
message(STATUS "Sample CoE Driver Configuration:")
message(STATUS "  HSL Directory: ${PDK_HSL_DIR}")
message(STATUS "  UDDF Include Directory: ${PDK_UDDF_DIR}")
message(STATUS "  Output Library: libsample_coe_library.so")
