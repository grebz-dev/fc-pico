# SPDX-License-Identifier: BSD-3-Clause
foreach(test_name test_mailbox test_tables test_sync test_rx test_host)
    add_executable(${test_name} ${test_name}.c)
    target_link_libraries(${test_name} PRIVATE fcbus_host)
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

add_executable(fcbus_layout_dump fcbus_layout_dump.c)
target_link_libraries(fcbus_layout_dump PRIVATE fcbus_core)
find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
    add_test(NAME layout_matches_descriptor
        COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/../tools/fcpico/layout.py
            --compare-native $<TARGET_FILE:fcbus_layout_dump>)
endif()
