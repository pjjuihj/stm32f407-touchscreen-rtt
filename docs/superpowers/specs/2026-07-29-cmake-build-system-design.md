# CMake 构建系统设计文档

## 概述

为 STM32F407 触摸屏项目添加 CMake 构建系统，使用 LVGL 官方 CMakeLists.txt，支持 VS Code Cortex-Debug 调试。**保留原有 Keil 项目，两套构建系统并存。**

## 目标

1. 添加 CMake 构建系统（与 Keil 并存）
2. 集成 LVGL 官方 CMakeLists.txt
3. 支持 VS Code Cortex-Debug 调试
4. 构建目录放在项目外，避免污染源码
5. 保留原有 Keil 项目文件不动

## 约束

- 调试器：CMSIS-DAP（v1 或 v2）
- 工具链：arm-none-eabi-gcc
- 构建目录：`触摸屏_usart_build/`（项目外）
- **不影响 Keil**：保留原有 Keil 项目文件（.uvprojx, .uvoptx 等），CMake 配置独立存放

## 架构

### 目录结构

```
触摸屏_usart/                    # 项目根目录
├── cmake_build/                 # CMake 构建配置（新建文件夹）
│   ├── CMakeLists.txt           # 主构建文件
│   ├── arm-none-eabi.cmake      # 工具链文件
│   └── .vscode/
│       ├── tasks.json           # 构建任务
│       └── launch.json          # 调试配置
├── lv_conf.h                    # LVGL 配置
├── LVGL/                        # LVGL 库（使用官方 CMakeLists.txt）
├── Main/                        # 主程序
├── USER/                        # 用户驱动
├── Common/                      # 公共代码
├── GUI/                         # GUI 驱动
├── STM32F4xx_HAL_Driver/        # HAL 库
├── Startup_config/              # 启动文件
├── TEST/                        # 测试代码
└── ...

触摸屏_usart_build/              # 构建输出目录（项目外）
```

### 主 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.20)

# 工具链设置（必须在 project() 之前）
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)

project(TOUCH C ASM)

# 编译选项
set(CPU_FLAGS "-mcpu=cortex-m4 -mthumb")
set(FPU_FLAGS "-mfpu=fpv4-sp-d16 -mfloat-abi=hard")
set(CMAKE_C_FLAGS "${CPU_FLAGS} ${FPU_FLAGS} -DUSE_HAL_DRIVER -DSTM32F407xx -O0 -Wall")
set(CMAKE_ASM_FLAGS "${CPU_FLAGS} -c")

# 包含 LVGL 作为子项目
set(LV_CONF_PATH "${CMAKE_SOURCE_DIR}/lv_conf.h" CACHE PATH "")
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
add_subdirectory(LVGL)

# 用户源文件
set(USER_SOURCES
    Main/main.c
    Main/stm32f4xx_it.c
    GUI/gui_driver.c
    USER/LED/led.c
    USER/BEEP/beep.c
    USER/KEY/key.c
    USER/LCD/lcd.c
    USER/TOUCH/touch.c
    USER/TOUCH/ft5426.c
    USER/TOUCH/xpt2046.c
    USER/USART/usart.c
    USER/LOG/log.c
    TEST/test_log.c
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

# 包含目录
include_directories(
    .
    Common
    Main
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
)

# 创建可执行文件
add_executable(${PROJECT_NAME}.elf
    ${USER_SOURCES}
    ${STARTUP_SOURCES}
    ${HAL_SOURCES}
)

# 链接 LVGL
target_link_libraries(${PROJECT_NAME}.elf lvgl)

# 链接选项
set_target_properties(${PROJECT_NAME}.elf PROPERTIES
    LINK_FLAGS "-T${CMAKE_SOURCE_DIR}/STM32F407VGTx_FLASH.ld --specs=nano.specs --specs=nosys.specs -Wl,--gc-sections -Wl,-Map=${PROJECT_NAME}.map"
)

# 后处理：生成 HEX, BIN
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND arm-none-eabi-objcopy -O ihex $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.hex
    COMMAND arm-none-eabi-objcopy -O binary $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.bin
    COMMAND arm-none-eabi-size $<TARGET_FILE:${PROJECT_NAME}.elf>
    COMMENT "Generating HEX and BIN files"
)
```

### 工具链文件

`cmake/arm-none-eabi.cmake`：

```cmake
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_SIZE arm-none-eabi-size)

set(CMAKE_C_FLAGS_INIT "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard")
set(CMAKE_ASM_FLAGS_INIT "-mcpu=cortex-m4 -mthumb")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
```

### VS Code 配置

**tasks.json**（放在 `cmake_build/.vscode/`）：

```json
{
  "version": "2.0.0",
  "tasks": [
    {
      "label": "CMake Configure",
      "type": "shell",
      "command": "cmake -B ${workspaceFolder}/../../触摸屏_usart_build -S ${workspaceFolder}",
      "group": "build"
    },
    {
      "label": "CMake Build",
      "type": "shell",
      "command": "cmake --build ${workspaceFolder}/../../触摸屏_usart_build",
      "group": { "kind": "build", "isDefault": true },
      "dependsOn": "CMake Configure"
    },
    {
      "label": "Clean",
      "type": "shell",
      "command": "rm -rf ${workspaceFolder}/../../触摸屏_usart_build",
      "group": "build"
    }
  ]
}
```

**launch.json**（放在 `cmake_build/.vscode/`）：

```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "Cortex-Debug (CMSIS-DAP)",
      "type": "cortex-debug",
      "request": "launch",
      "servertype": "openocd",
      "device": "STM32F407VGTx",
      "configFiles": [
        "interface/cmsis-dap.cfg",
        "target/stm32f4x.cfg"
      ],
      "executable": "${workspaceFolder}/../../触摸屏_usart_build/TOUCH.elf",
      "svdFile": "${workspaceFolder}/../STM32F407.svd",
      "runToEntryPoint": "main"
    }
  ]
}
```

## 构建命令

```bash
# 进入 cmake_build 目录
cd cmake_build

# 配置（构建目录在项目外）
cmake -B ../../触摸屏_usart_build -S .

# 构建
cmake --build ../../触摸屏_usart_build

# 清理
rm -rf ../../触摸屏_usart_build
```

## 验证方法

1. 运行 cmake 配置，确认无错误
2. 运行 cmake --build，确认编译成功
3. 检查生成的 TOUCH.elf, TOUCH.hex 文件
4. 使用 arm-none-eabi-size 查看大小
5. 在 VS Code 中配置 Cortex-Debug，测试调试功能

## 依赖

- arm-none-eabi-gcc 工具链
- CMake 3.20+
- VS Code + Cortex-Debug 扩展
- OpenOCD（用于调试）

## 风险

1. **中文路径问题**：之前 CMake 因中文路径失败，需要使用相对路径
2. **LVGL 版本兼容性**：LVGL 9.5.0 的 CMakeLists.txt 可能需要适配
3. **OpenOCD 配置**：CMSIS-DAP 的 OpenOCD 配置可能需要调整
4. **Keil 兼容性**：CMake 配置完全独立，不影响 Keil 项目

## 后续步骤

1. 创建 `cmake_build/` 文件夹
2. 创建 `cmake_build/CMakeLists.txt`
3. 创建 `cmake_build/arm-none-eabi.cmake`
4. 创建 `cmake_build/.vscode/tasks.json`
5. 创建 `cmake_build/.vscode/launch.json`
6. 测试构建
7. 测试调试
