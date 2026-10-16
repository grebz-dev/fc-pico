# SPDX-License-Identifier: BSD-3-Clause
#
# Host build of the shared platform libraries, the emulator cartridge model and
# their tests:
#   cmake -S platform -B build/platform && cmake --build build/platform
#   ctest --test-dir build/platform --output-on-failure
cmake_minimum_required(VERSION 3.16)
project(fcpico_platform C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Werror)
endif()

enable_testing()

# The libraries are also linked into emulator plugins (shared objects).
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory(fcbus)
add_subdirectory(${PROJECT_SOURCE_DIR}/../sim/cartmodel cartmodel)

add_subdirectory(tests)

# One shared library for the Python tools (tools/fcpico/native.py): the bus core with
# its layout and stream encoding, and the cartridge model with its providers.
set(FCPICO_CARTMODEL ${PROJECT_SOURCE_DIR}/../sim/cartmodel)
add_library(fcpico_native SHARED
    fcbus/fcbus_core.c fcbus/fcbus_layout.c fcbus/fcbus_pack.c fcbus/fcbus_host.c
    ${FCPICO_CARTMODEL}/fcpico_cart.c
    ${FCPICO_CARTMODEL}/providers/pattern.c
    ${FCPICO_CARTMODEL}/providers/demo.c)
target_include_directories(fcpico_native PRIVATE fcbus ${FCPICO_CARTMODEL})
