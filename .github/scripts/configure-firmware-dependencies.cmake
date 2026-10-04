# CI-only integration for the checked-in SEGGER RTT library.
# The Keil/EIDE projects already provide these include paths and source file;
# the root CMake target does not currently list them.
if(NOT PROJECT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    return()
endif()

set(_segger_rtt_root "${CMAKE_SOURCE_DIR}/SEGGER_RTT")
set(_segger_rtt_source "${_segger_rtt_root}/RTT/SEGGER_RTT.c")

if(NOT EXISTS "${_segger_rtt_root}/SEGGER_RTT.h" OR NOT EXISTS "${_segger_rtt_source}")
    message(FATAL_ERROR "The checked-in SEGGER RTT library is incomplete")
endif()

include_directories(
    "${_segger_rtt_root}"
    "${_segger_rtt_root}/RTT"
)

# The project creates TOUCH.elf after project(); attach RTT at the end of the
# top-level directory so this remains limited to the CI build configuration.
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}"
    CALL target_sources "${PROJECT_NAME}.elf" PRIVATE "${_segger_rtt_source}"
)
