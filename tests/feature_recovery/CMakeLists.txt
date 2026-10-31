cmake_minimum_required(VERSION 3.20)
project(feature_recovery_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Werror)

enable_testing()

# Keep compiler input paths ASCII-only; several Windows GCC builds cannot create
# nested object paths containing this repository's Chinese directory name.
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/src")
foreach(source_header IN ITEMS
        ../../IoT/wifi_at.c ../../IoT/wifi_at.h
        ../../IoT/wifi_state.c ../../IoT/wifi_state.h
        ../../IoT/wifi.c ../../IoT/wifi.h
        ../../IoT/wifi_uart_rx_recovery.c ../../IoT/wifi_uart_rx_recovery.h
        ../../IoT/wifi_tcp_ipd_parser.c ../../IoT/wifi_tcp_ipd_parser.h
        ../../IoT/mqtt_client.c ../../IoT/mqtt_client.h
        ../../IoT/mqtt_json.c ../../IoT/mqtt_json.h
        ../../IoT/mqtt_wire.c ../../IoT/mqtt_wire.h
        ../../IoT/iot.c ../../IoT/iot.h
        ../../IoT/bluetooth.c ../../IoT/bluetooth.h
        ../../IoT/bluetooth_command_parser.c ../../IoT/bluetooth_command_parser.h
        ../../IoT/board_io.h
        ../../Main/console.c ../../Main/console.h
        ../../GUI/gui_driver.c
        ../../USER/USART/usart1_tx_scheduler.c ../../USER/USART/usart1_tx_scheduler.h
        ../../USER/USART/usart1_rx_recovery.c ../../USER/USART/usart1_rx_recovery.h)
    get_filename_component(source_name "${source_header}" NAME)
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/${source_header}"
                   "${CMAKE_CURRENT_BINARY_DIR}/src/${source_name}" COPYONLY)
endforeach()

add_executable(test_wifi_at
    test_wifi_at.c
    "${CMAKE_CURRENT_BINARY_DIR}/src/wifi_at.c"
)
target_include_directories(test_wifi_at PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/src")
add_test(NAME wifi_at COMMAND test_wifi_at)

add_executable(test_wifi_state
    test_wifi_state.c
    "${CMAKE_CURRENT_BINARY_DIR}/src/wifi_state.c"
)
target_include_directories(test_wifi_state PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/src")
add_test(NAME wifi_state COMMAND test_wifi_state)

add_executable(test_usart1_tx_scheduler
    test_usart1_tx_scheduler.c
    "${CMAKE_CURRENT_BINARY_DIR}/src/usart1_tx_scheduler.c"
)
target_include_directories(test_usart1_tx_scheduler PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/src")
add_test(NAME usart1_tx_scheduler COMMAND test_usart1_tx_scheduler)

add_executable(test_usart1_rx_recovery
    test_usart1_rx_recovery.c
    "${CMAKE_CURRENT_BINARY_DIR}/src/usart1_rx_recovery.c"
)
target_include_directories(test_usart1_rx_recovery PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/src")
add_test(NAME usart1_rx_recovery COMMAND test_usart1_rx_recovery)

add_executable(test_wifi_uart_rx_recovery
    test_wifi_uart_rx_recovery.c
    "${CMAKE_CURRENT_BINARY_DIR}/src/wifi_uart_rx_recovery.c"
)
target_include_directories(test_wifi_uart_rx_recovery PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/src")
add_test(NAME wifi_uart_rx_recovery COMMAND test_wifi_uart_rx_recovery)

add_executable(test_wifi_tcp_ipd_parser
    test_wifi_tcp_ipd_parser.c
    "${CMAKE_CURRENT_BINARY_DIR}/src/wifi_tcp_ipd_parser.c"
)
target_include_directories(test_wifi_tcp_ipd_parser PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/src")
add_test(NAME wifi_tcp_ipd_parser COMMAND test_wifi_tcp_ipd_parser)

add_executable(test_iot_layout test_iot_layout.c)
target_include_directories(test_iot_layout PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/src")
add_test(NAME iot_layout COMMAND test_iot_layout)

add_executable(test_ble_command_parser
    test_ble_command_parser.c
    "${CMAKE_CURRENT_BINARY_DIR}/src/bluetooth_command_parser.c"
)
target_include_directories(test_ble_command_parser PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/src")
add_test(NAME ble_command_parser COMMAND test_ble_command_parser)

add_executable(test_bluetooth_runtime
    test_bluetooth_runtime.c
    "${CMAKE_CURRENT_BINARY_DIR}/src/bluetooth.c"
)
target_include_directories(test_bluetooth_runtime PRIVATE
    fakes
    "${CMAKE_CURRENT_BINARY_DIR}/src"
)
add_test(NAME bluetooth_runtime COMMAND test_bluetooth_runtime)

add_executable(test_console_bt_status
    test_console_bt_status.c
    "${CMAKE_CURRENT_BINARY_DIR}/src/console.c"
    "${CMAKE_CURRENT_BINARY_DIR}/src/usart1_rx_recovery.c"
)
target_include_directories(test_console_bt_status PRIVATE
    console_fakes
    fakes
    "${CMAKE_CURRENT_BINARY_DIR}/src"
)
add_test(NAME console_bt_status COMMAND test_console_bt_status)

add_executable(test_iot_ble
    test_iot_ble.c
    "${CMAKE_CURRENT_BINARY_DIR}/src/iot.c"
    "${CMAKE_CURRENT_BINARY_DIR}/src/bluetooth_command_parser.c"
)
target_compile_definitions(test_iot_ble PRIVATE IOT_MQTT_ENABLE=0)
target_include_directories(test_iot_ble PRIVATE
    fakes
    "${CMAKE_CURRENT_BINARY_DIR}/src"
)
add_test(NAME iot_ble COMMAND test_iot_ble)

add_executable(test_mqtt_core
    test_mqtt_core.c
    "${CMAKE_CURRENT_BINARY_DIR}/src/mqtt_client.c"
    "${CMAKE_CURRENT_BINARY_DIR}/src/mqtt_json.c"
    "${CMAKE_CURRENT_BINARY_DIR}/src/mqtt_wire.c"
)
set_source_files_properties(
    "${CMAKE_CURRENT_BINARY_DIR}/src/mqtt_client.c"
    PROPERTIES COMPILE_OPTIONS "-Wno-unused-function"
)
target_include_directories(test_mqtt_core PRIVATE
    fakes
    "${CMAKE_CURRENT_BINARY_DIR}/src"
)
add_test(NAME mqtt_core COMMAND test_mqtt_core)

add_executable(test_iot_auto_tcp
    test_iot_auto_tcp.c
    "${CMAKE_CURRENT_BINARY_DIR}/src/iot.c"
    "${CMAKE_CURRENT_BINARY_DIR}/src/bluetooth_command_parser.c"
)
target_include_directories(test_iot_auto_tcp PRIVATE
    fakes
    "${CMAKE_CURRENT_BINARY_DIR}/src"
)
target_compile_definitions(test_iot_auto_tcp PRIVATE
    IOT_AUTO_TCP_DIAGNOSTIC_ENABLE=1
    IOT_MQTT_ENABLE=0
)
add_test(NAME iot_auto_tcp COMMAND test_iot_auto_tcp)

add_executable(test_wifi_takeover
    test_wifi_takeover.c
    "${CMAKE_CURRENT_BINARY_DIR}/src/wifi.c"
    "${CMAKE_CURRENT_BINARY_DIR}/src/wifi_at.c"
    "${CMAKE_CURRENT_BINARY_DIR}/src/wifi_state.c"
    "${CMAKE_CURRENT_BINARY_DIR}/src/wifi_uart_rx_recovery.c"
    "${CMAKE_CURRENT_BINARY_DIR}/src/wifi_tcp_ipd_parser.c"
)
target_include_directories(test_wifi_takeover PRIVATE
    fakes
    ../../IoT
    "${CMAKE_CURRENT_BINARY_DIR}/src"
)
add_test(NAME wifi_takeover COMMAND test_wifi_takeover)

add_executable(test_gui_driver_init
    test_gui_driver_init.c
    "${CMAKE_CURRENT_BINARY_DIR}/src/gui_driver.c"
)
target_include_directories(test_gui_driver_init BEFORE PRIVATE
    gui_fakes
    ../../GUI
)
add_test(NAME gui_driver_init COMMAND test_gui_driver_init)

find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(NAME startup_order
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_startup_order.py
                ${CMAKE_CURRENT_SOURCE_DIR}/../../Main/main.c)
    add_test(NAME hil_serial_port
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../hil/test_serial_port.py)
    add_test(NAME hil_takeover_protocol
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../hil/test_takeover_protocol.py)
endif()
