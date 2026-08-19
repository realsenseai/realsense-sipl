# CMakeLists.txt — D457 GMSL UDDF camera-module driver (single-library form)
#
# Produces libuddf_d457cameramodule_library.so → /usr/lib/nvsipl_drv. SIPL scans that
# dir, calls uddf_discover_drivers(), and matches DriverInfo.name "D457" to the JSON
# `moduleDriverName` in ../d457_gmsl.json.
#
# This compiles the D457 sensor + module + library into one .so and links the stock
# MAX9295 serializer driver target. It mirrors the SDK targets:
#   sensor: uddf/drivers/sensors/AR0234  (add_library sensor_<x>_driver)
#   module: uddf/coSerDes/modules/<X>    (links sensor + serializer driver targets)
#   library:uddf/libraries/module<X>     (SHARED .so from <X>Library.cpp)
# collapsed here for a self-contained drop-in. (Split into the 3 canonical dirs later if
# you want the D457 sensor reusable across modules.)
#
# INTEGRATION (on the rig, in the extracted SDK):
#   cp -r driver /usr/src/jetson_sipl_api/sipl/uddf/cdd_d457
#   cp ../d457_ds5_registers.h /usr/src/jetson_sipl_api/sipl/uddf/cdd_d457/
#   # in sipl/CMakeLists.txt add (after the MAX9295 subdir, ~line 112):
#   #   add_subdirectory(uddf/cdd_d457)
#   # and add nvuddf_d457 to the all_drivers custom target's DEPENDS (~line 303).
#   cd sipl && mkdir -p build && cd build && cmake .. && make -j uddf_d457cameramodule_library
#   sudo make install     # or: sudo cp <built>.so /usr/lib/nvsipl_drv/

cmake_minimum_required(VERSION 3.15)

# Fallbacks if the parent CMake didn't export these (cdd_d457 sits at sipl/uddf/cdd_d457).
if(NOT DEFINED UDDF_CDD_DRIVER)
    set(UDDF_CDD_DRIVER ${CMAKE_CURRENT_SOURCE_DIR}/../drivers)
endif()
if(NOT DEFINED UDDF_CDD_COSERDES)
    set(UDDF_CDD_COSERDES ${CMAKE_CURRENT_SOURCE_DIR}/../coSerDes)
endif()

add_library(uddf_d457cameramodule_library SHARED
    D457Sensor.cpp
    D457Module.cpp
    D457Library.cpp
)
add_library(uddf-cdd::uddf_d457cameramodule_library ALIAS uddf_d457cameramodule_library)

target_compile_features(uddf_d457cameramodule_library PUBLIC cxx_std_17)

target_include_directories(uddf_d457cameramodule_library
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}          # D457*.hpp + d457_ds5_registers.h (copied alongside)
        ${UDDF_CDD_DRIVER}                   # serializers/MAX9295/MAX9295.hpp, common/SensorUbb.hpp
        ${UDDF_CDD_COSERDES}                 # common/ModuleUbb.hpp
)

target_link_libraries(uddf_d457cameramodule_library
    PUBLIC
        uddf::uddf
        uddf-cdd::ser_max9295_driver         # stock MAX9295 serializer (built by the SDK)
)

# No HSL codegen for the D457 (DS5) side — it uses dynamic HSL (II2CBuilder) built at
# runtime from d457_ds5_registers.h. The MAX9295 driver supplies its own HSL.

install(TARGETS uddf_d457cameramodule_library
    LIBRARY DESTINATION /usr/lib/nvsipl_drv
    COMPONENT camera_drivers
)
