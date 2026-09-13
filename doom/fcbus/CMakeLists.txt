# SPDX-License-Identifier: BSD-3-Clause
#
# fcbus -- cartridge bus library. Backend-independent core plus a host simulation
# backend today; mirrors tutorial_project/tuto1_hw/sys/rp_system.{cpp,h} (see README.md).

add_library(fcbus_core STATIC
    fcbus_core.c
)
target_include_directories(fcbus_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(fcbus_host STATIC
    fcbus_host.c
)
target_link_libraries(fcbus_host PUBLIC fcbus_core)
target_include_directories(fcbus_host PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(NOT FCPICO_HOST_ONLY)
    add_library(fcbus_device STATIC fcbus_device.c)
    target_include_directories(fcbus_device PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(fcbus_device PUBLIC
        fcbus_core hardware_dma hardware_gpio hardware_irq hardware_pio pico_time)
    pico_generate_pio_header(fcbus_device ${CMAKE_CURRENT_SOURCE_DIR}/fcppu.pio)
endif()
