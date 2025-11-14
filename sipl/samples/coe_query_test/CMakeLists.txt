# SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: LicenseRef-NvidiaProprietary
#
# NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
# property and proprietary rights in and to this material, related
# documentation and any modifications thereto. Any use, reproduction,
# disclosure or distribution of this material and related documentation
# without an express license agreement from NVIDIA CORPORATION or
# its affiliates is strictly prohibited.

project(nvsipl_coe_query_test)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions -frtti")

set(SIPL_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../include")
set(SIPL_QUERY_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../include/query/include")

# Include directories matching Makefile.tmk
include_directories(
    ${SIPL_INCLUDE_DIR}
    ${SIPL_QUERY_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Find JsonCpp
find_path(JSONCPP_INCLUDE_DIR json/json.h
    PATHS 
        /usr/include/jsoncpp
        /usr/local/include/jsoncpp
        ../../../3rdparty/jsoncpp/include
)

find_library(JSONCPP_LIBRARY 
    NAMES jsoncpp
    PATHS 
        /usr/lib
        /usr/local/lib
)

# Find SIPL Query library
find_library(
    NVSIPL_QUERY_LIB nvsipl_query
    HINTS /usr/lib/aarch64-linux-gnu/nvidia/
)

if(NVSIPL_QUERY_LIB)
    add_executable(${PROJECT_NAME} main.cpp)
    
    target_link_libraries(${PROJECT_NAME} 
        ${NVSIPL_QUERY_LIB}
        stdc++
    )
    
    if(JSONCPP_LIBRARY)
        target_link_libraries(${PROJECT_NAME} ${JSONCPP_LIBRARY})
        target_include_directories(${PROJECT_NAME} PRIVATE ${JSONCPP_INCLUDE_DIR})
    endif()
else()
    message(WARNING "nvsipl_query library not found. Skipping ${PROJECT_NAME}.")
endif() 