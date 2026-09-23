# SPDX-License-Identifier: BSD-3-Clause
#
# doom/CMakeLists.txt -- top-level build.
#
# FCPICO_HOST_ONLY builds the backend-independent pieces without pico-sdk. The device branch
# imports pico-sdk and builds the RP2350 bus backend and test-pattern firmware.

cmake_minimum_required(VERSION 3.20)

option(FCPICO_HOST_ONLY "Host libraries and tests only, no pico-sdk" OFF)
option(FCPICO_BUILD_COSIM "Build the host cartridge model for MesenCE" OFF)
option(FCPICO_BUILD_ENGINE "Configure the RP2350 Doom engine platform target" OFF)

if(FCPICO_BUILD_COSIM AND NOT FCPICO_HOST_ONLY)
    message(FATAL_ERROR "FCPICO_BUILD_COSIM requires FCPICO_HOST_ONLY=ON")
endif()

if(FCPICO_HOST_ONLY)
    project(fcpico_doom C)

    set(CMAKE_C_STANDARD 11)
    set(CMAKE_C_STANDARD_REQUIRED ON)
    set(CMAKE_C_EXTENSIONS OFF)
    # Host libraries also link into MesenCore.so for co-simulation.
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)

    add_compile_options(-Wall -Wextra -Werror)

    add_subdirectory(fcbus)
    add_subdirectory(testpattern)

    if(FCPICO_BUILD_COSIM)
        add_subdirectory(sim/cartmodel)
    endif()

    enable_testing()
    add_subdirectory(tests)
else()
    set(PICO_BOARD fcpico CACHE STRING "FC PICO RP2350 board")
    set(PICO_PLATFORM rp2350-arm-s CACHE STRING "pico-sdk platform")
    list(APPEND PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/cmake/boards)

    if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/rp2040-doom/pico_sdk_import.cmake)
        include(${CMAKE_CURRENT_LIST_DIR}/rp2040-doom/pico_sdk_import.cmake)
    elseif(DEFINED ENV{PICO_SDK_PATH})
        include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
    elseif(PICO_SDK_PATH)
        include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)
    else()
        message(FATAL_ERROR "Set PICO_SDK_PATH to a pico-sdk 2.1.1 checkout")
    endif()

    project(fcpico_doom C CXX ASM)
    set(CMAKE_C_STANDARD 11)
    set(CMAKE_C_STANDARD_REQUIRED ON)
    pico_sdk_init()

    set(FCPICO_SUPERBUILD 1)
    add_subdirectory(fcbus)
    add_subdirectory(testpattern)
    if(FCPICO_BUILD_ENGINE)
        add_subdirectory(port/apu)
        add_subdirectory(port/video)
        add_subdirectory(rp2040-doom EXCLUDE_FROM_ALL)
    endif()
    add_subdirectory(port)
endif()
