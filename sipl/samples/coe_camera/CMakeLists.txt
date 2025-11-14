# SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: LicenseRef-NvidiaProprietary
#
# NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
# property and proprietary rights in and to this material, related
# documentation and any modifications thereto. Any use, reproduction,
# disclosure or distribution of this material and related documentation
# without an express license agreement from NVIDIA CORPORATION or
# its affiliates is strictly prohibited.

project(nvsipl_coe_camera)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions -frtti")

set(SIPL_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../include")
set(SIPL_QUERY_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../include/query/include")
set(NVSCI_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../include/nvsci")
set(NVMEDIA_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../include/nvmedia")

# Include directories matching Makefile.tmk
include_directories(
    ${SIPL_INCLUDE_DIR}
    ${SIPL_QUERY_INCLUDE_DIR}
    ${NVSCI_INCLUDE_DIR}
    ${NVMEDIA_INCLUDE_DIR}
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

# Find required libraries
find_library(NVSIPL_QUERY_LIB nvsipl_query
    HINTS /usr/lib/aarch64-linux-gnu/nvidia/
)
find_library(NVSIPL_CORE_LIB nvsipl
    HINTS /usr/lib/aarch64-linux-gnu/nvidia/
)
find_library(NVSCIBUF_LIB nvscibuf
    HINTS /usr/lib/aarch64-linux-gnu/nvidia/
)
find_library(NVSCISYNC_LIB nvscisync
    HINTS /usr/lib/aarch64-linux-gnu/nvidia/
)

if(NVSIPL_CORE_LIB AND NVSIPL_QUERY_LIB)
    add_executable(${PROJECT_NAME} 
        main.cpp
        CUtils.cpp
    )
    
    target_link_libraries(${PROJECT_NAME} 
        ${NVSIPL_CORE_LIB}
        ${NVSIPL_QUERY_LIB}
        stdc++
    )
    
    # Link optional libraries if found
    if(JSONCPP_LIBRARY)
        target_link_libraries(${PROJECT_NAME} ${JSONCPP_LIBRARY})
        target_include_directories(${PROJECT_NAME} PRIVATE ${JSONCPP_INCLUDE_DIR})
    endif()
    
    if(NVSCIBUF_LIB)
        target_link_libraries(${PROJECT_NAME} ${NVSCIBUF_LIB})
    endif()
    
    if(NVSCISYNC_LIB)
        target_link_libraries(${PROJECT_NAME} ${NVSCISYNC_LIB})
    endif()
    
    if(NVSYSUTILS_LIB)
        target_link_libraries(${PROJECT_NAME} ${NVSYSUTILS_LIB})
    endif()
else()
    message(WARNING "Required SIPL libraries not found. Skipping ${PROJECT_NAME}.")
endif() 