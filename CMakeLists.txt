cmake_minimum_required(VERSION 3.20)

# 工具链设置（必须在 project() 之前）
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
# The CMake compiler-id link probe needs the bare-metal syscall stubs too.
set(CMAKE_C_FLAGS_INIT "--specs=nosys.specs")

project(TOUCH C ASM)

# 编译选项
set(CPU_FLAGS "-mcpu=cortex-m4 -mthumb")
set(FPU_FLAGS "-mfpu=fpv4-sp-d16 -mfloat-abi=hard")
set(CMAKE_C_FLAGS "${CPU_FLAGS} ${FPU_FLAGS} -DUSE_HAL_DRIVER -DSTM32F407xx -O0 -Wall -Wno-unused-variable -Wno-unused-but-set-variable")
set(CMAKE_ASM_FLAGS "${CPU_FLAGS} -c")

# LVGL 配置（必须在 add_subdirectory 之前）
set(LV_BUILD_CONF_PATH "${CMAKE_SOURCE_DIR}/lv_conf.h" CACHE PATH "")
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_PRIVATE_API OFF CACHE BOOL "" FORCE)
set(LV_BUILD_LVGL_H_SIMPLE_INCLUDE OFF CACHE BOOL "" FORCE)

# 禁用不需要的 LVGL 特性（减少编译文件）
set(LV_USE_DRAW_SW_ASM_NONE ON CACHE BOOL "" FORCE)
set(LV_USE_NATIVE_HELIUM_ASM OFF CACHE BOOL "" FORCE)
set(LV_USE_NATIVE_NEON_ASM OFF CACHE BOOL "" FORCE)
set(LV_USE_NATIVE_RISCV_ASM OFF CACHE BOOL "" FORCE)

# This checkout contains LVGL's source tree but not its optional env_support
# CMake integration directory. Build the portable C sources directly instead
# of relying on the missing desktop-only helper.
file(GLOB_RECURSE LVGL_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/LVGL/src/*.c"
)
list(FILTER LVGL_SOURCES EXCLUDE REGEX "/drivers/")
add_library(lvgl STATIC ${LVGL_SOURCES})
target_include_directories(lvgl PUBLIC
    "${CMAKE_SOURCE_DIR}/LVGL"
    "${CMAKE_SOURCE_DIR}/LVGL/src"
    "${CMAKE_SOURCE_DIR}"
)
target_compile_definitions(lvgl PUBLIC LV_CONF_INCLUDE_SIMPLE=1)

# 用户源文件（使用相对路径）
set(USER_SOURCES
    Main/main.c
    Main/console.c
    Main/stm32f4xx_it.c
    GUI/gui_driver.c
    USER/LED/led.c
    USER/BEEP/beep.c
    USER/KEY/key.c
    USER/LCD/lcd.c
    USER/LCD/lcd_backlight.c
    USER/TOUCH/touch.c
    USER/TOUCH/ft5426.c
    USER/TOUCH/xpt2046.c
    USER/USART/usart.c
    USER/USART/usart1_tx_scheduler.c
    USER/USART/usart1_rx_recovery.c
    USER/LOG/log.c
    TEST/test_log.c
    IoT/iot.c
    IoT/bluetooth.c
    IoT/bluetooth_command_parser.c
    IoT/board_io.c
    IoT/wifi.c
    IoT/wifi_at.c
    IoT/wifi_state.c
    IoT/wifi_uart_rx_recovery.c
    SEGGER_RTT/RTT/SEGGER_RTT.c
    SEGGER_RTT/Sys/SEGGER_SYSVIEW.c
    Common/common.c
    Startup_config/system_stm32f4xx.c
    Startup_config/stm32f4xx_hal_msp.c
)

# 启动文件
set(STARTUP_SOURCES
    Startup_config/startup_stm32f407xx_gcc.s
)

# HAL 源文件
set(HAL_SOURCES
    STM32F4xx_HAL_Driver/src/stm32f4xx_hal.c
    STM32F4xx_HAL_Driver/src/stm32f4xx_hal_cortex.c
    STM32F4xx_HAL_Driver/src/stm32f4xx_hal_gpio.c
    STM32F4xx_HAL_Driver/src/stm32f4xx_hal_pwr.c
    STM32F4xx_HAL_Driver/src/stm32f4xx_hal_pwr_ex.c
    STM32F4xx_HAL_Driver/src/stm32f4xx_hal_rcc.c
    STM32F4xx_HAL_Driver/src/stm32f4xx_hal_rcc_ex.c
    STM32F4xx_HAL_Driver/src/stm32f4xx_hal_dma.c
    STM32F4xx_HAL_Driver/src/stm32f4xx_hal_dma_ex.c
    STM32F4xx_HAL_Driver/src/stm32f4xx_hal_uart.c
    STM32F4xx_HAL_Driver/src/stm32f4xx_hal_sram.c
    STM32F4xx_HAL_Driver/src/stm32f4xx_ll_fsmc.c
    STM32F4xx_HAL_Driver/src/stm32f4xx_hal_flash.c
    STM32F4xx_HAL_Driver/src/stm32f4xx_hal_flash_ex.c
)

# 创建可执行文件
add_executable(${PROJECT_NAME}.elf
    ${USER_SOURCES}
    ${STARTUP_SOURCES}
    ${HAL_SOURCES}
)

# 包含目录（使用相对路径）
target_include_directories(${PROJECT_NAME}.elf PRIVATE
    .
    Common
    Main
    IoT
    Startup_config
    STM32F4xx_HAL_Driver/inc
    USER/LED
    USER/LCD
    USER/BEEP
    USER/KEY
    USER/TOUCH
    USER/USART
    USER/LOG
    TEST
    GUI
    SEGGER_RTT
    SEGGER_RTT/RTT
    SEGGER_RTT/Sys
)

# Enable the requested automatic one-shot TCP command for the F407 firmware.
# The source keeps this diagnostic feature disabled for builds that do not opt in.
target_compile_definitions(${PROJECT_NAME}.elf PRIVATE
    IOT_AUTO_TCP_DIAGNOSTIC_ENABLE=1
)

# 链接 LVGL
target_link_libraries(${PROJECT_NAME}.elf lvgl)

# 链接选项
set_target_properties(${PROJECT_NAME}.elf PROPERTIES
    LINK_FLAGS "-T${CMAKE_SOURCE_DIR}/Project/STM32F407VGTx_FLASH.ld --specs=nano.specs --specs=nosys.specs -Wl,--gc-sections -Wl,-Map=${PROJECT_NAME}.map"
)

# 后处理：生成 HEX, BIN
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE_NAME:${PROJECT_NAME}.elf> ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE_NAME:${PROJECT_NAME}.elf> ${PROJECT_NAME}.bin
    COMMENT "Generating HEX and BIN files"
)
