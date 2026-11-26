# Purpose: build RYB Comms, its host tests, and with -DRYB_MODULE=<name> that module's program.
# Owning module: management (mgmt/).

cmake_minimum_required(VERSION 3.19)
project(ryb_comms LANGUAGES C)

set(RYB_MODULE "" CACHE STRING "Module this board runs: heartbeat, sound, decision or motor")
set_property(CACHE RYB_MODULE PROPERTY STRINGS "" heartbeat sound decision motor)
set(RYB_GPIO_BACKEND mmio CACHE STRING "GPIO backend for RYB_MODULE: mmio (PYNQ) or sim (host)")
set_property(CACHE RYB_GPIO_BACKEND PROPERTY STRINGS mmio sim)
option(RYB_BUILD_TESTS "Build the RYB Comms host tests" ON)

include("${CMAKE_CURRENT_LIST_DIR}/cmake/RybComms.cmake")

foreach(module IN LISTS RYB_MODULE)
    ryb_add_module(${module})
endforeach()

if(RYB_BUILD_TESTS)
    enable_testing()

    add_executable(ryb_test_packet test/test_packet.c ryb_packet.c)
    target_include_directories(ryb_test_packet PRIVATE "${RYB_INCLUDE_DIR}")
    _ryb_warnings(ryb_test_packet)
    add_test(NAME ryb_packet COMMAND ryb_test_packet)

    # The loopback tests run the real bit loop over the sim backend, so they need POSIX threads.
    if(UNIX)
        ryb_comms_add_library(ryb_comms_loopback_command MODULE decision
            JSON test/loopback_command.json BACKEND sim)
        add_executable(ryb_test_loopback_command test/test_loopback.c)
        target_link_libraries(ryb_test_loopback_command PRIVATE ryb_comms_loopback_command)
        _ryb_warnings(ryb_test_loopback_command)
        add_test(NAME ryb_loopback_command COMMAND ryb_test_loopback_command)

        ryb_comms_add_library(ryb_comms_loopback_appended MODULE heartbeat
            JSON test/loopback_appended.json BACKEND sim)
        add_executable(ryb_test_loopback_appended test/test_loopback.c)
        target_link_libraries(ryb_test_loopback_appended PRIVATE ryb_comms_loopback_appended)
        _ryb_warnings(ryb_test_loopback_appended)
        add_test(NAME ryb_loopback_appended COMMAND ryb_test_loopback_appended)
    endif()
endif()
