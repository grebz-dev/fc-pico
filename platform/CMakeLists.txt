# SPDX-License-Identifier: BSD-3-Clause
#
# Host build of the shared platform libraries and their tests:
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

add_subdirectory(fcbus)

enable_testing()
add_subdirectory(tests)
