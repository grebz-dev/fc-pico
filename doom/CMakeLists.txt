# SPDX-License-Identifier: BSD-3-Clause
#
# doom/CMakeLists.txt -- top-level build.
#
# Full picture (device firmware + engine + host simulation) is the superbuild sketched in
# doom/plan/08-build.md, task P0-T1. Until that lands, FCPICO_HOST_ONLY builds just the
# backend-independent, pico-sdk-free pieces: fcbus's core + host backend and their tests
# (doom/plan/10-workplan.md, P0-T5/P0-T6).

cmake_minimum_required(VERSION 3.20)

option(FCPICO_HOST_ONLY "Host libraries and tests only, no pico-sdk" OFF)

if(FCPICO_HOST_ONLY)
    project(fcpico_doom C)

    set(CMAKE_C_STANDARD 11)
    set(CMAKE_C_STANDARD_REQUIRED ON)
    set(CMAKE_C_EXTENSIONS OFF)

    add_compile_options(-Wall -Wextra -Werror)

    add_subdirectory(fcbus)

    enable_testing()
    add_subdirectory(tests)
else()
    message(FATAL_ERROR
        "doom/CMakeLists.txt: only -DFCPICO_HOST_ONLY=ON is implemented so far. The full "
        "superbuild (pico-sdk, the engine submodule, the device firmware targets) is task "
        "P0-T1 ('Superbuild skeleton and LED-blink firmware') in doom/plan/10-workplan.md "
        "and has not been done yet. Configure with -DFCPICO_HOST_ONLY=ON for the "
        "backend-independent fcbus core, host backend and their tests.")

    # TODO(P0-T1): once the superbuild lands, this branch becomes the real thing sketched
    # in doom/plan/08-build.md ("Superbuild"):
    #
    # cmake_minimum_required(VERSION 3.20)
    # set(PICO_BOARD fcpico CACHE STRING "")              # doom/cmake/boards/fcpico.h:
    #                                                      #   pico2 + 4 MB flash + pins
    # set(PICO_PLATFORM rp2350-arm-s CACHE STRING "")     # or "host" for the simulation build
    # include(rp2040-doom/pico_sdk_import.cmake)          # honours PICO_SDK_PATH / FETCH_FROM_GIT
    # project(fcpico_doom C CXX ASM)
    # pico_sdk_init()
    # set(FCPICO_SUPERBUILD 1)                            # tells the engine tree not to init the SDK again
    # add_subdirectory(fcbus)
    # add_subdirectory(port/apu)
    # add_subdirectory(rp2040-doom EXCLUDE_FROM_ALL)      # engine libraries + doom_tiny_fcpico target
    # add_subdirectory(port)                              # fcpico_doom (UF2), fcpico_testpattern (UF2)
    # if (FCPICO_BUILD_TESTS)
    #     enable_testing()
    #     add_subdirectory(tests)
    # endif()
endif()
