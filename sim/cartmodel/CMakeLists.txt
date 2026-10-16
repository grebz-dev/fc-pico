# SPDX-License-Identifier: BSD-3-Clause
add_library(fcpico_cartmodel STATIC
    fcpico_cart.c
    providers/pattern.c
    providers/demo.c
)
target_include_directories(fcpico_cartmodel PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(fcpico_cartmodel PUBLIC fcbus_host)

add_executable(test_cartmodel tests/test_cartmodel.c)
target_include_directories(test_cartmodel PRIVATE ${PROJECT_SOURCE_DIR}/tests)
target_link_libraries(test_cartmodel PRIVATE fcpico_cartmodel)
add_test(NAME test_cartmodel COMMAND test_cartmodel)
