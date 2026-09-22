# SPDX-License-Identifier: BSD-3-Clause
add_library(fcvideo STATIC fcvideo.c)
target_include_directories(fcvideo PUBLIC ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_LIST_DIR}/../../fcbus)
target_compile_options(fcvideo PRIVATE -Wall -Wextra -Werror)
