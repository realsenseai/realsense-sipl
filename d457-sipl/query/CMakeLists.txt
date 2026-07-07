# CMakeLists.txt — D457 SIPL query-database plugin
#
# Produces libnvsipl_qry_d457.so → /usr/lib/nvsipl_drv. At startup SIPL's stock
# libnvsipl_query.so (CNvMQuery::ParseDatabase) dlopen()s every libnvsipl_qry_*.so in
# that dir and dlsym()s `const char* CNvMQuery_GetJsonData(void)` to read each plugin's
# camera-config JSON. That is how `nvsipl_camera -c D457_Camera` resolves our camera.
#
# Unlike ../driver (the UDDF module driver, which must build INSIDE the extracted SDK
# tree against uddf::uddf), this plugin is fully self-contained: one source, no SIPL
# headers, no link deps — it just returns a JSON string literal. So it builds anywhere
# with an aarch64 C++ toolchain, no SDK checkout required.
#
# BUILD & INSTALL (on the Thor rig, or any aarch64 host):
#   cd query && cmake -B build && cmake --build build
#   sudo cmake --install build          # → /usr/lib/nvsipl_drv/libnvsipl_qry_d457.so
#
# NOTE: only d457_query.cpp is compiled. traceon.cpp in this dir is an unrelated
# LD_PRELOAD trace shim (a dead end, kept for reference) and is intentionally excluded.

cmake_minimum_required(VERSION 3.15)
project(nvsipl_qry_d457 LANGUAGES CXX)

# Target name nvsipl_qry_d457 → output libnvsipl_qry_d457.so (CMake adds lib/.so).
add_library(nvsipl_qry_d457 SHARED d457_query.cpp)

target_compile_features(nvsipl_qry_d457 PRIVATE cxx_std_17)

# CNvMQuery_GetJsonData must be exported. The symbol is extern "C" with default
# visibility; do NOT compile this target with -fvisibility=hidden or SIPL's dlsym fails.

install(TARGETS nvsipl_qry_d457
    LIBRARY DESTINATION /usr/lib/nvsipl_drv
)
