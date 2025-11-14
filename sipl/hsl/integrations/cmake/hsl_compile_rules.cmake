# SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: LicenseRef-NvidiaProprietary
#
# NVIDIA CORPORATION, its affiliates and licensors retain all intellectual property and proprietary
# rights in and to this material, related documentation and any modifications thereto. Any use,
# reproduction, disclosure or distribution of this material and related documentation without an
# express license agreement from NVIDIA CORPORATION or its affiliates is strictly prohibited.

include(CMakeParseArguments)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# User-configurable HSL Paths
option(HSL_PYTHON_SCRIPT_DIR
       "Path to the directory containing HSL Python scripts (hslcompile.py, drvhsl.py, etc.)" ""
)
option(
  HSL_PYTHON_MODULE_DIR
  "Path to the directory containing the compiled HSL Python extension module (nvhslencoderpy*.so)" ""
)

# hsl_add_driver_sources
#
# Configures custom commands to compile HSL (.hsl) files into C++ header (.hpp) files. For each
# input file specified in SOURCES, a corresponding .hpp file will be generated with the same
# basename in the HSL_OUTPUT_SUBDIR.
#
# Usage: hsl_add_driver_sources(<TARGET> SOURCES <hsl_source1.hsl> [<hsl_source2.hsl> ...] # List
# of input .hsl files [OUTPUT_SUBDIR <subdir>] # Subdir in build dir (default: hsl_gen)
# [CONFIG_PARAMS <param1=val1> ...] # -D defines for hslcompile.py (applied to all sources)
# [NAMESPACE <cpp_namespace>] # -n namespace for drvhsl.py (applied to all generated headers)
# [CONSTANT_PREFIX <prefix>]  # -p prefix for drvhsl.py constants (applied to all)
# [DEPENDENCIES <dep1.py> ...] # Additional files that HSL sources depend on )
#
# Example: hsl_add_driver_sources(my_driver_target
#                              SOURCES sequence1.hsl scriptA.hsl
#                              NAMESPACE my_driver::hsl
#                              DEPENDENCIES data_utils.py constants.py)
#
function(hsl_add_driver_sources TARGET)
  set(options "")
  set(oneValueArgs OUTPUT_SUBDIR NAMESPACE CONSTANT_PREFIX)
  set(multiValueArgs SOURCES CONFIG_PARAMS DEPENDENCIES)

  cmake_parse_arguments(HSL "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  # --- Argument Validation ---
  if(NOT HSL_SOURCES)
    message(
      FATAL_ERROR "hsl_add_driver_sources: SOURCES argument (list of .hsl files) is required."
    )
  endif()

  if(NOT TARGET ${TARGET})
    message(FATAL_ERROR "hsl_add_driver_sources: Target '${TARGET}' does not exist.")
  endif()

  # --- Determine HSL Paths (Prioritize Options > FetchContent > Error) ---
  set(LOCAL_HSL_SCRIPT_DIR "")
  set(LOCAL_HSL_PYBIND11_LIB_DIR "")
  set(HSL_PATH_SOURCE "")
  if(HSL_PYTHON_SCRIPT_DIR AND HSL_PYTHON_MODULE_DIR)
    message(STATUS "HSL Compile: Using HSL paths provided via CMake options.")
    set(LOCAL_HSL_SCRIPT_DIR ${HSL_PYTHON_SCRIPT_DIR})
    set(LOCAL_HSL_PYBIND11_LIB_DIR ${HSL_PYTHON_MODULE_DIR})
    set(HSL_PATH_SOURCE "Options")
  elseif(
    DEFINED hsl_SOURCE_DIR
    AND IS_DIRECTORY ${hsl_SOURCE_DIR}
    AND DEFINED hsl_BINARY_DIR
  )
    message(
      STATUS
        "HSL Compile: Deriving HSL paths from FetchContent variables (hsl_SOURCE_DIR/hsl_BINARY_DIR)."
    )
    set(LOCAL_HSL_SCRIPT_DIR ${hsl_SOURCE_DIR}/src/pyhsl/src)
    set(LOCAL_HSL_PYBIND11_LIB_DIR ${hsl_BINARY_DIR}/src/bytecode/encoder/binding)
    set(HSL_PATH_SOURCE "FetchContent")
  else()
    message(
      FATAL_ERROR
        "HSL Compile: Could not determine HSL paths. Build HSL using FetchContent or set HSL_PYTHON_SCRIPT_DIR and HSL_PYTHON_MODULE_DIR options."
    )
  endif()
  if(NOT IS_DIRECTORY ${LOCAL_HSL_SCRIPT_DIR})
    message(
      FATAL_ERROR
        "HSL Compile: HSL script directory '${LOCAL_HSL_SCRIPT_DIR}' (Source: ${HSL_PATH_SOURCE}) is not valid."
    )
  endif()
  if((HSL_PATH_SOURCE STREQUAL "Options") AND NOT IS_DIRECTORY ${LOCAL_HSL_PYBIND11_LIB_DIR})
    message(
      FATAL_ERROR
        "HSL Compile: HSL module directory '${LOCAL_HSL_PYBIND11_LIB_DIR}' (Source: ${HSL_PATH_SOURCE}) is not valid."
    )
  endif()

  # --- Set Defaults & Common Paths ---
  if(NOT HSL_OUTPUT_SUBDIR)
    set(HSL_OUTPUT_SUBDIR "hsl_gen")
  endif()
  set(HSL_SCRIPT_DIR ${LOCAL_HSL_SCRIPT_DIR})
  set(HSLCOMPILE_SCRIPT ${HSL_SCRIPT_DIR}/hslcompile.py)
  set(DRVHSL_SCRIPT ${HSL_SCRIPT_DIR}/drvhsl.py)
  set(HSL_PYBIND11_LIB_DIR ${LOCAL_HSL_PYBIND11_LIB_DIR})
  set(HSL_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/${HSL_OUTPUT_SUBDIR})

  # Common Python environment
  set(HSL_PYTHON_EXECUTABLE ${Python3_EXECUTABLE})
  set(HSL_PYTHON_PATH_SEP ":")
  set(HSL_PYTHONPATH "${HSL_SCRIPT_DIR}${HSL_PYTHON_PATH_SEP}${HSL_PYBIND11_LIB_DIR}")

  # Common hslcompile.py -D arguments (if any)
  set(COMMON_HSLCOMPILE_DEFINES)
  foreach(param ${HSL_CONFIG_PARAMS})
    list(APPEND COMMON_HSLCOMPILE_DEFINES -D ${param})
  endforeach()

  # Add common include directory for all generated headers
  target_include_directories(${TARGET} PRIVATE ${HSL_OUTPUT_DIR})

  # Process additional dependencies - convert to absolute paths
  set(HSL_DEPENDENCY_FILES)
  foreach(DEP ${HSL_DEPENDENCIES})
    set(CURRENT_DEP_PATH ${CMAKE_CURRENT_SOURCE_DIR}/${DEP})
    if(NOT EXISTS "${CURRENT_DEP_PATH}")
      message(
        FATAL_ERROR
          "HSL Compile: Dependency file '${CURRENT_DEP_PATH}' does not exist."
      )
    endif()
    list(APPEND HSL_DEPENDENCY_FILES ${CURRENT_DEP_PATH})
  endforeach()

  # --- Loop through each HSL source file ---
  foreach(CURRENT_HSL_SOURCE ${HSL_SOURCES})
    get_filename_component(DERIVED_BASENAME ${CURRENT_HSL_SOURCE} NAME_WE)
    set(CURRENT_ABS_HSL_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/${CURRENT_HSL_SOURCE})

    if(NOT EXISTS "${CURRENT_ABS_HSL_SOURCE}")
      message(
        FATAL_ERROR
          "HSL Compile: Input HSL source file '${CURRENT_ABS_HSL_SOURCE}' does not exist."
      )
    endif()
    if(IS_DIRECTORY "${CURRENT_ABS_HSL_SOURCE}")
      message(
        FATAL_ERROR
          "HSL Compile: Input HSL source '${CURRENT_ABS_HSL_SOURCE}' must be a file, not a directory."
      )
    endif()

    set(CURRENT_HSLC_FILE ${HSL_OUTPUT_DIR}/${DERIVED_BASENAME}.hslc)
    set(CURRENT_HPP_FILE ${HSL_OUTPUT_DIR}/${DERIVED_BASENAME}.hpp)

    # Prepare hslcompile.py Arguments for this source
    set(CURRENT_HSLCOMPILE_ARGS -i ${CURRENT_ABS_HSL_SOURCE} -o ${CURRENT_HSLC_FILE})
    if(COMMON_HSLCOMPILE_DEFINES)
      list(APPEND CURRENT_HSLCOMPILE_ARGS ${COMMON_HSLCOMPILE_DEFINES})
    endif()

    # Prepare drvhsl.py Arguments for this source
    set(CURRENT_DRVHSL_ARGS -i ${CURRENT_HSLC_FILE} -d ${HSL_OUTPUT_DIR} -b ${DERIVED_BASENAME})
    if(HSL_NAMESPACE)
      list(APPEND CURRENT_DRVHSL_ARGS -n ${HSL_NAMESPACE})
    endif()
    if(HSL_CONSTANT_PREFIX)
      list(APPEND CURRENT_DRVHSL_ARGS -p ${HSL_CONSTANT_PREFIX})
    endif()

    set(HSLCOMPILE_CUSTOM_COMMAND_DEPS ${CURRENT_ABS_HSL_SOURCE} ${HSL_DEPENDENCY_FILES})
    if(TARGET nvhslencoderpy)
      # If nvhslencoderpy exists (e.g. we've found it from FetchContent), add it as a dependency. It
      # is crucial that the encoder is built before the HSL is compiled.
      list(APPEND HSLCOMPILE_CUSTOM_COMMAND_DEPS nvhslencoderpy)
    elseif(HSL_PATH_SOURCE STREQUAL "FetchContent")
      # If HSL is from FetchContent, nvhslencoderpy should ideally exist. Warn if not.
      message(
        WARNING
          "HSL Compile: HSL from FetchContent, but target 'nvhslencoderpy' not found. HSL compilation for ${CURRENT_HSL_SOURCE} might fail if Python module is missing."
      )
    endif()

    # Custom Command for hslcompile.py
    add_custom_command(
      OUTPUT ${CURRENT_HSLC_FILE}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${HSL_OUTPUT_DIR} # Ensure dir exists
      COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${HSL_PYTHONPATH} ${HSL_PYTHON_EXECUTABLE}
              ${HSLCOMPILE_SCRIPT} ${CURRENT_HSLCOMPILE_ARGS}
      DEPENDS ${HSLCOMPILE_CUSTOM_COMMAND_DEPS}
      COMMENT "Compiling HSL ${CURRENT_HSL_SOURCE} -> ${DERIVED_BASENAME}.hslc"
      VERBATIM
    )

    # Custom Command for drvhsl.py
    add_custom_command(
      OUTPUT ${CURRENT_HPP_FILE}
      COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${HSL_PYTHONPATH} ${HSL_PYTHON_EXECUTABLE}
              ${DRVHSL_SCRIPT} ${CURRENT_DRVHSL_ARGS}
      DEPENDS ${CURRENT_HSLC_FILE}
      COMMENT "Generating HSL Header ${DERIVED_BASENAME}.hslc -> ${DERIVED_BASENAME}.hpp"
      VERBATIM
    )

    # Target Integration for this specific generated header
    set(CURRENT_HSL_CUSTOM_TARGET_NAME ${TARGET}_${DERIVED_BASENAME}_hsl_gen)
    add_custom_target(${CURRENT_HSL_CUSTOM_TARGET_NAME} ALL DEPENDS ${CURRENT_HPP_FILE})
    add_dependencies(${TARGET} ${CURRENT_HSL_CUSTOM_TARGET_NAME})

  endforeach()

endfunction()
