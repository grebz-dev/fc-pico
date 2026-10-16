# SPDX-License-Identifier: BSD-3-Clause
foreach(test_name test_mailbox test_tables test_sync test_rx test_host)
    add_executable(${test_name} ${test_name}.c)
    target_link_libraries(${test_name} PRIVATE fcbus_host)
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
