# SPDX-License-Identifier: BSD-3-Clause
add_subdirectory(../../port/video ${CMAKE_CURRENT_BINARY_DIR}/fcvideo)
add_executable(test_fcvideo test_fcvideo.c)
target_link_libraries(test_fcvideo PRIVATE fcvideo)
target_include_directories(test_fcvideo PRIVATE ..)
add_test(NAME test_fcvideo COMMAND test_fcvideo)
