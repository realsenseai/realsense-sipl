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
  fusa-pdk-samples
  VERSION 1.0.0
  DESCRIPTION "FUSA PDK Sample Drivers - CoE and EagleAIO"
  LANGUAGES CXX
)

# Set the HSL and UDDF directories relative to this CMakeLists.txt
set(PDK_HSL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/hsl")
set(PDK_UDDF_DIR "${CMAKE_CURRENT_SOURCE_DIR}/uddf")

# Verify that HSL and UDDF directories exist
if(NOT EXISTS "${PDK_HSL_DIR}")
    message(FATAL_ERROR "HSL directory not found at ${PDK_HSL_DIR}")
endif()

if(NOT EXISTS "${PDK_UDDF_DIR}")
    message(FATAL_ERROR "UDDF directory not found at ${PDK_UDDF_DIR}")
endif()

# Set HSL Python paths
set(HSL_PYTHON_SCRIPT_DIR "${PDK_HSL_DIR}/pyhsl")
set(HSL_PYTHON_MODULE_DIR "${PDK_HSL_DIR}/pyhsl")

# Add HSL CMake modules to the module path
list(APPEND CMAKE_MODULE_PATH "${PDK_HSL_DIR}/integrations/cmake")

# Include HSL compilation rules
include(hsl_compile_rules)

# Print configuration information
message(STATUS "=== FUSA PDK Samples Configuration ===")
message(STATUS "Project Name:        ${PROJECT_NAME}")
message(STATUS "Project Version:     ${PROJECT_VERSION}")
message(STATUS "HSL Directory:       ${PDK_HSL_DIR}")
message(STATUS "UDDF Directory:      ${PDK_UDDF_DIR}")
message(STATUS "Build Type:          ${CMAKE_BUILD_TYPE}")
message(STATUS "=====================================")

# Add sample drivers subdirectories
add_subdirectory(uddf/samples/drivers/coe)
add_subdirectory(uddf/samples/drivers/eagleAIO)
add_subdirectory(uddf/samples/drivers/realsense)
add_subdirectory(samples/coe_camera)
add_subdirectory(samples/coe_query_test)

# Create a custom target that depends on all sample drivers
add_custom_target(samples ALL
    DEPENDS
        sample_coe_library
        nvuddf_eagle_library
        nvuddf_realsense_library
    COMMENT "Building all FUSA PDK sample drivers"
)

# Add optional dependencies if targets exist
if(TARGET nvsipl_coe_camera)
    add_dependencies(samples nvsipl_coe_camera)
endif()

if(TARGET nvsipl_coe_query_test)
    add_dependencies(samples nvsipl_coe_query_test)
endif()

# Install targets for the shared libraries
install(TARGETS sample_coe_library
    LIBRARY DESTINATION /usr/lib/nvsipl_uddf
    COMPONENT sample_drivers
)

install(TARGETS nvuddf_eagle_library
    LIBRARY DESTINATION /usr/lib/nvsipl_uddf
    COMPONENT sample_drivers
)

install(TARGETS nvuddf_realsense_library
    LIBRARY DESTINATION /usr/lib/nvsipl_uddf
    COMPONENT sample_drivers
)

# Install sample applications only if they were built
if(TARGET nvsipl_coe_camera)
    install(TARGETS nvsipl_coe_camera
        RUNTIME DESTINATION /usr/bin
        COMPONENT sample_apps
    )
endif()

if(TARGET nvsipl_coe_query_test)
    install(TARGETS nvsipl_coe_query_test
        RUNTIME DESTINATION /usr/bin
        COMPONENT sample_apps
    )
endif()

# Print build summary
message(STATUS "")
message(STATUS "=== Build Targets ===")
message(STATUS "Available targets:")
message(STATUS "  - sample_coe_library: CoE sample driver")
message(STATUS "  - nvuddf_eagle_library: EagleAIO sample driver")
message(STATUS "  - nvuddf_realsense_library: Realsense sample driver")
message(STATUS "  - nvsipl_coe_camera: CoE camera sample application")
message(STATUS "  - nvsipl_coe_query_test: CoE query test sample application")
message(STATUS "  - samples: Build all sample drivers and applications")
message(STATUS "  - install: Install libraries to /usr/lib/nvsipl_uddf/ and apps to /usr/bin/")
message(STATUS "=====================")
message(STATUS "")
message(STATUS "To build all samples: make samples")
message(STATUS "To build individual drivers: make <target_name>")
message(STATUS "To install libraries: sudo make install")
