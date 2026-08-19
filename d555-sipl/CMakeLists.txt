# SPDX-FileCopyrightText: Copyright (c) 2025-2026 RealSense AI. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.15 FATAL_ERROR)

project(
  d555_sipl_driver
  VERSION 1.0.0
  DESCRIPTION "RealSense D555 camera-over-Ethernet driver for NVIDIA SIPL/UDDF"
  LANGUAGES CXX
)

# The NVIDIA Jetson Camera SIPL SDK is NOT vendored here (it is NVIDIA-licensed and cannot be
# redistributed). Build against the copy installed on the Jetson, or point SIPL_ROOT elsewhere.
set(SIPL_ROOT "/usr/src/jetson_sipl_api/sipl" CACHE PATH "Jetson Camera SIPL SDK source root")

if(NOT EXISTS "${SIPL_ROOT}/uddf/include")
  message(FATAL_ERROR
    "Jetson SIPL SDK not found at SIPL_ROOT=${SIPL_ROOT}. "
    "Install the Camera SIPL API package on the Jetson, or pass -DSIPL_ROOT=/path/to/sipl.")
endif()

# CoEModuleDriverBase.hpp / HsbTransportDriverBase.hpp moved between L4T releases:
# R38 ships them in uddf/samples/drivers/coe, R39.2 in uddf/common/helpers.
find_path(SIPL_COE_BASE_DIR
  NAMES HsbTransportDriverBase.hpp
  PATHS "${SIPL_ROOT}/uddf/samples/drivers/coe" "${SIPL_ROOT}/uddf/common/helpers"
  NO_DEFAULT_PATH)
if(NOT SIPL_COE_BASE_DIR)
  message(FATAL_ERROR "HsbTransportDriverBase.hpp not found under ${SIPL_ROOT}")
endif()

find_library(NVHSL_ENCODER_LIB
  NAMES nvhslencoder
  PATHS "${SIPL_ROOT}/hsl/lib-target"
  NO_DEFAULT_PATH)
if(NOT NVHSL_ENCODER_LIB)
  message(FATAL_ERROR "libnvhslencoder.so not found under ${SIPL_ROOT}/hsl/lib-target")
endif()

set(PDK_HSL_DIR "${SIPL_ROOT}/hsl")
set(HSL_PYTHON_SCRIPT_DIR "${PDK_HSL_DIR}/pyhsl")
set(HSL_PYTHON_MODULE_DIR "${PDK_HSL_DIR}/pyhsl")
list(APPEND CMAKE_MODULE_PATH "${PDK_HSL_DIR}/integrations/cmake")
include(hsl_compile_rules)

add_subdirectory(hololink/core)

add_library(nvuddf_realsense_library SHARED
    uddf_driver/RealsenseLibrary.cpp
    uddf_driver/HsbTransportDriver.cpp
    uddf_driver/D555ModuleDriver.cpp
    uddf_driver/D555SensorDriver.cpp
)

target_compile_features(nvuddf_realsense_library PRIVATE cxx_std_17)

target_include_directories(nvuddf_realsense_library PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/uddf_driver
    ${CMAKE_CURRENT_SOURCE_DIR}/hololink/core
    ${PDK_HSL_DIR}/include
    ${SIPL_ROOT}/uddf/include
    ${SIPL_COE_BASE_DIR}
)

target_link_libraries(nvuddf_realsense_library PRIVATE
    ${NVHSL_ENCODER_LIB}
    hololink_core_realsense
)

hsl_add_driver_sources(nvuddf_realsense_library
  SOURCES uddf_driver/d555_hsl.py
  NAMESPACE uddf::samples::d555::hsl
  DEPENDENCIES uddf_driver/d555_data.py
)

message(STATUS "D555 SIPL driver configuration:")
message(STATUS "  SIPL_ROOT:        ${SIPL_ROOT}")
message(STATUS "  CoE base headers: ${SIPL_COE_BASE_DIR}")
message(STATUS "  HSL encoder:      ${NVHSL_ENCODER_LIB}")
message(STATUS "  Output:           libnvuddf_realsense_library.so")
