cmake_minimum_required(VERSION 3.20)

# 工具链设置（必须在 project() 之前）
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(TOUCH C ASM)

# 编译选项
set(CPU_FLAGS "-mcpu=cortex-m4 -mthumb")
set(FPU_FLAGS "-mfpu=fpv4-sp-d16 -mfloat-abi=hard")
set(CMAKE_C_FLAGS "${CPU_FLAGS} ${FPU_FLAGS} -DUSE_HAL_DRIVER -DSTM32F407xx -O0 -Wall -Wno-unused-variable -Wno-unused-but-set-variable")
set(CMAKE_ASM_FLAGS "${CPU_FLAGS} -c")

# LVGL 配置（必须在 add_subdirectory 之前）
set(LV_BUILD_CONF_PATH "${CMAKE_SOURCE_DIR}/../lv_conf.h" CACHE PATH "")
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

# 排除 LVGL 中不需要的 .S 汇编文件
set_source_files_properties(${LVGL_ROOT_DIR}/src/draw/sw/blend/helium/lv_blend_helium.S PROPERTIES LANGUAGE C)

# 包含 LVGL 作为子项目
add_subdirectory(${CMAKE_SOURCE_DIR}/../LVGL ${CMAKE_BINARY_DIR}/LVGL)

# 排除 LVGL 中不需要的 .S 汇编文件（helium/neon）
if(TARGET lvgl)
    get_target_property(_lvgl_sources lvgl SOURCES)
    # 移除所有 .S 文件
    list(FILTER _lvgl_sources EXCLUDE REGEX ".*\\.S$")
    set_target_properties(lvgl PROPERTIES SOURCES "${_lvgl_sources}")
endif()

# 用户源文件
set(USER_SOURCES
    ${CMAKE_SOURCE_DIR}/../Main/main.c
    ${CMAKE_SOURCE_DIR}/../Main/stm32f4xx_it.c
    ${CMAKE_SOURCE_DIR}/../GUI/gui_driver.c
    ${CMAKE_SOURCE_DIR}/../USER/LED/led.c
    ${CMAKE_SOURCE_DIR}/../USER/BEEP/beep.c
    ${CMAKE_SOURCE_DIR}/../USER/KEY/key.c
    ${CMAKE_SOURCE_DIR}/../USER/LCD/lcd.c
    ${CMAKE_SOURCE_DIR}/../USER/TOUCH/touch.c
    ${CMAKE_SOURCE_DIR}/../USER/TOUCH/ft5426.c
    ${CMAKE_SOURCE_DIR}/../USER/TOUCH/xpt2046.c
    ${CMAKE_SOURCE_DIR}/../USER/USART/usart.c
    ${CMAKE_SOURCE_DIR}/../USER/LOG/log.c
    ${CMAKE_SOURCE_DIR}/../TEST/test_log.c
    ${CMAKE_SOURCE_DIR}/../Common/common.c
    ${CMAKE_SOURCE_DIR}/../Startup_config/system_stm32f4xx.c
    ${CMAKE_SOURCE_DIR}/../Startup_config/stm32f4xx_hal_msp.c
)

# 启动文件
set(STARTUP_SOURCES
    ${CMAKE_SOURCE_DIR}/../Startup_config/startup_stm32f407xx_gcc.s
)

# HAL 源文件
set(HAL_SOURCES
    ${CMAKE_SOURCE_DIR}/../STM32F4xx_HAL_Driver/src/stm32f4xx_hal.c
    ${CMAKE_SOURCE_DIR}/../STM32F4xx_HAL_Driver/src/stm32f4xx_hal_cortex.c
    ${CMAKE_SOURCE_DIR}/../STM32F4xx_HAL_Driver/src/stm32f4xx_hal_gpio.c
    ${CMAKE_SOURCE_DIR}/../STM32F4xx_HAL_Driver/src/stm32f4xx_hal_pwr.c
    ${CMAKE_SOURCE_DIR}/../STM32F4xx_HAL_Driver/src/stm32f4xx_hal_pwr_ex.c
    ${CMAKE_SOURCE_DIR}/../STM32F4xx_HAL_Driver/src/stm32f4xx_hal_rcc.c
    ${CMAKE_SOURCE_DIR}/../STM32F4xx_HAL_Driver/src/stm32f4xx_hal_rcc_ex.c
    ${CMAKE_SOURCE_DIR}/../STM32F4xx_HAL_Driver/src/stm32f4xx_hal_dma.c
    ${CMAKE_SOURCE_DIR}/../STM32F4xx_HAL_Driver/src/stm32f4xx_hal_dma_ex.c
    ${CMAKE_SOURCE_DIR}/../STM32F4xx_HAL_Driver/src/stm32f4xx_hal_uart.c
    ${CMAKE_SOURCE_DIR}/../STM32F4xx_HAL_Driver/src/stm32f4xx_hal_sram.c
    ${CMAKE_SOURCE_DIR}/../STM32F4xx_HAL_Driver/src/stm32f4xx_ll_fsmc.c
    ${CMAKE_SOURCE_DIR}/../STM32F4xx_HAL_Driver/src/stm32f4xx_hal_flash.c
    ${CMAKE_SOURCE_DIR}/../STM32F4xx_HAL_Driver/src/stm32f4xx_hal_flash_ex.c
)

# 创建可执行文件
add_executable(${PROJECT_NAME}.elf
    ${USER_SOURCES}
    ${STARTUP_SOURCES}
    ${HAL_SOURCES}
)

# 使用 target_include_directories 替代 include_directories
target_include_directories(${PROJECT_NAME}.elf PRIVATE
    ${CMAKE_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/../Common
    ${CMAKE_SOURCE_DIR}/../Main
    ${CMAKE_SOURCE_DIR}/../Startup_config
    ${CMAKE_SOURCE_DIR}/../STM32F4xx_HAL_Driver/inc
    ${CMAKE_SOURCE_DIR}/../USER/LED
    ${CMAKE_SOURCE_DIR}/../USER/LCD
    ${CMAKE_SOURCE_DIR}/../USER/BEEP
    ${CMAKE_SOURCE_DIR}/../USER/KEY
    ${CMAKE_SOURCE_DIR}/../USER/TOUCH
    ${CMAKE_SOURCE_DIR}/../USER/USART
    ${CMAKE_SOURCE_DIR}/../USER/LOG
    ${CMAKE_SOURCE_DIR}/../TEST
    ${CMAKE_SOURCE_DIR}/../GUI
)

# 链接 LVGL
target_link_libraries(${PROJECT_NAME}.elf lvgl)

# 链接选项
set_target_properties(${PROJECT_NAME}.elf PROPERTIES
    LINK_FLAGS "-T${CMAKE_SOURCE_DIR}/../Project/STM32F407VGTx_FLASH.ld --specs=nano.specs --specs=nosys.specs -Wl,--gc-sections -Wl,-Map=${PROJECT_NAME}.map"
)

# 后处理：生成 HEX, BIN
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND arm-none-eabi-objcopy -O ihex $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.hex
    COMMAND arm-none-eabi-objcopy -O binary $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.bin
    COMMAND arm-none-eabi-size $<TARGET_FILE:${PROJECT_NAME}.elf>
    COMMENT "Generating HEX and BIN files"
)
