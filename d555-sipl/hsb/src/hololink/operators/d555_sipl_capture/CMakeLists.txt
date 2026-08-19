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

add_library(d555_sipl_capture STATIC
    d555_sipl_capture.cpp
)

set_property(TARGET d555_sipl_capture PROPERTY POSITION_INDEPENDENT_CODE ON)

add_library(hololink::operators::d555_sipl_capture ALIAS d555_sipl_capture)

target_link_directories(d555_sipl_capture
  PUBLIC
    /usr/lib/aarch64-linux-gnu/nvidia
)

target_link_libraries(d555_sipl_capture
  PUBLIC
    hololink
  PRIVATE
    holoscan::core
    nvbufsurface
    nvfusacapinterface
    nvrm_surface
    nvscibuf
    nvscisync
    nvsipl
    nvsipl_query
)

target_include_directories(d555_sipl_capture
  PUBLIC
    /usr/src/jetson_sipl_api/sipl/include
    /usr/src/jetson_sipl_api/sipl/include/nvsci
    /usr/src/jetson_sipl_api/sipl/include/query/include
)
