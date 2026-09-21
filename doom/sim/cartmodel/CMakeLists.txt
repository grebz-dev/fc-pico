# SPDX-License-Identifier: BSD-3-Clause
#
# Host cartridge model used by the Mesen mapper.  It can also be configured on
# its own for adapter-only tests: cmake -S doom/sim/cartmodel -B /tmp/cartmodel.

cmake_minimum_required(VERSION 3.20)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    project(fcpico_cartmodel C)
    set(CMAKE_C_STANDARD 11)
    set(CMAKE_C_STANDARD_REQUIRED ON)
    set(CMAKE_C_EXTENSIONS OFF)
    set(FCPICO_HOST_ONLY ON)
endif()

if(NOT TARGET fcbus_host)
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../../fcbus" "${CMAKE_CURRENT_BINARY_DIR}/fcbus")
endif()

add_library(fcpico_cartmodel STATIC fcpico_cart.c)
target_include_directories(fcpico_cartmodel PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(fcpico_cartmodel PUBLIC fcbus_host)
target_compile_options(fcpico_cartmodel PRIVATE -Wall -Wextra -Werror)
