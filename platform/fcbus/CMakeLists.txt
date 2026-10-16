# SPDX-License-Identifier: BSD-3-Clause
add_library(fcbus_core STATIC fcbus_core.c fcbus_layout.c)
target_include_directories(fcbus_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(fcbus_host STATIC fcbus_host.c)
target_link_libraries(fcbus_host PUBLIC fcbus_core)
