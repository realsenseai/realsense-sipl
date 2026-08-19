# SPDX-FileCopyrightText: Copyright (c) 2025-2026 RealSense AI. All rights reserved.
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

add_library(d457_sipl_capture STATIC
    d457_sipl_capture.cpp
)

set_property(TARGET d457_sipl_capture PROPERTY POSITION_INDEPENDENT_CODE ON)

add_library(hololink::operators::d457_sipl_capture ALIAS d457_sipl_capture)

target_link_directories(d457_sipl_capture
  PUBLIC
    /usr/lib/aarch64-linux-gnu/nvidia
)

target_link_libraries(d457_sipl_capture
  PUBLIC
    hololink::core
    holoscan::core
  PRIVATE
    GXF::multimedia
    nvbufsurface
    nvfusacapinterface
    nvrm_surface
    nvscibuf
    nvscisync
    nvsipl
    nvsipl_query
)

target_include_directories(d457_sipl_capture
  PUBLIC
    /usr/src/jetson_sipl_api/sipl/include
    /usr/src/jetson_sipl_api/sipl/include/nvsci
    /usr/src/jetson_sipl_api/sipl/include/query/include
)

# Installation of the d457_sipl_capture operator
set_target_properties(d457_sipl_capture PROPERTIES EXPORT_NAME operators::d457_sipl_capture)
install(TARGETS d457_sipl_capture
        EXPORT HololinkTargets
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT hololink-operators)

install(FILES
          ${CMAKE_CURRENT_SOURCE_DIR}/d457_sipl_capture.hpp
          ${CMAKE_CURRENT_SOURCE_DIR}/d457_sipl_fmt.hpp
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/hololink/operators/d457_sipl_capture
        COMPONENT hololink-operators)
