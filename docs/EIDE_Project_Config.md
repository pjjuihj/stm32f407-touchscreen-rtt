# EIDE 项目配置报告

> 项目：触摸屏_usart (TOUCH)
> MCU：STM32F407VGTx (Cortex-M4F)
> 配置时间：2026-07-31

---

## 📋 配置概览

| 项目 | 配置值 |
|------|--------|
| **项目名称** | TOUCH |
| **EIDE 版本** | v4.1 |
| **MCU 型号** | STM32F407VGTx |
| **CPU 类型** | Cortex-M4 |
| **FPU** | 单精度浮点 (fpv4-sp-d16) |
| **编译器** | GCC ARM (arm-none-eabi-gcc) |
| **链接脚本** | STM32F407VGTx_FLASH.ld |
| **烧录器** | PyOCD (CMSIS-DAP) |
| **调试器** | Cortex-Debug |

---

## 📁 配置文件结构

```
Project/
├── .eide/
│   ├── eide.yml              # 主配置文件 ✅ 已修复设备名称
│   ├── env.ini               # Keil 环境变量
│   ├── files.options.yml     # 文件选项（空）
│   └── lvgl_sources.yml      # LVGL 源文件配置
├── TOUCH.code-workspace      # VSCode 工作空间
├── TOUCH.uvprojx             # Keil 项目文件
├── Makefile                  # GCC Makefile
└── STM32F407VGTx_FLASH.ld    # 链接脚本
```

---

## 🔧 已修复的问题

### 1. 设备名称不匹配

**问题**：`eide.yml` 中设备名称为 `STM32F407ZETx`，但实际项目使用 `STM32F407VGTx`

**修复**：已将 `deviceName` 从 `STM32F407ZETx` 改为 `STM32F407VGTx`

---

## 🛠️ 工具链配置

### GCC ARM 工具链路径

```
D:\arm-gnu-toolchain-15.2\arm-gnu-toolchain-15.2.rel1-mingw-w64-i686-arm-none-eabi\bin\
```

### 编译器参数

| 参数 | 值 |
|------|-----|
| **CPU** | `-mcpu=cortex-m4 -mthumb` |
| **FPU** | `-mfpu=fpv4-sp-d16 -mfloat-abi=hard` |
| **优化** | `-O0` (调试) |
| **警告** | `-Wall` |
| **定义** | `USE_HAL_DRIVER`, `STM32F407xx` |

---

## 📂 源文件配置

### 主要源文件组

| 组名 | 文件 |
|------|------|
| **Main** | `main.c`, `stm32f4xx_it.c` |
| **Startup_config** | `system_stm32f4xx.c`, `stm32f4xx_hal_msp.c`, `startup_stm32f407xx_gcc.s` |
| **USER** | LED, BEEP, KEY, LCD, TOUCH, USART, LOG 驱动 |
| **STM32F4_HAL_FWLIB** | HAL 库源文件 |
| **Common** | `common.c` |
| **GUI** | `gui_driver.c` |
| **UI** | `ui_main.c` |
| **LVGL** | LVGL 9.5.0 完整源文件 |

### 包含路径

```
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
LVGL
LVGL/src
LVGL/src/drivers/display/ili9341
GUI
ui
```

---

## 💾 内存配置

| 区域 | 起始地址 | 大小 |
|------|----------|------|
| **Flash (IROM)** | `0x08000000` | 1024 KB |
| **RAM (IRAM)** | `0x20000000` | 128 KB |
| **CCMRAM** | `0x10000000` | 64 KB |

---

## 🔌 烧录器配置

### PyOCD 配置

| 参数 | 值 |
|------|-----|
| **目标** | `cortex_m` |
| **速度** | 4MHz |
| **基地址** | `0x08000000` |
| **接口** | SWD |

### OpenOCD 配置（备选）

| 参数 | 值 |
|------|-----|
| **接口** | cmsis-dap |
| **目标** | stm32f4x |
| **基地址** | `0x08000000` |

---

## 🚀 使用说明

### 1. 打开项目

在 VSCode 中打开 `Project/TOUCH.code-workspace` 文件

### 2. 配置工具链

1. 打开 VSCode 设置 (Ctrl+,)
2. 搜索 `EIDE`
3. 设置 `EIDE.ARM.GCC.InstallDirectory` 为：
   ```
   D:\arm-gnu-toolchain-15.2\arm-gnu-toolchain-15.2.rel1-mingw-w64-i686-arm-none-eabi
   ```

### 3. 编译项目

1. 打开 EIDE 侧边栏
2. 在 **OPERATIONS** 视图中点击 **Build**
3. 或使用快捷键 `Ctrl+Shift+B`

### 4. 烧录程序

1. 确保 CMSIS-DAP 调试器已连接
2. 在 **OPERATIONS** 视图中点击 **Upload**
3. 或使用快捷键 `Ctrl+Shift+U`

### 5. 调试项目

1. 安装 **Cortex-Debug** 插件
2. 按 F5 启动调试
3. 调试器会自动从烧录配置生成 launch.json

---

## ⚠️ 注意事项

1. **不要手动修改 `.eide/eide.json`**（EIDE v4.x 使用 `eide.yml`）
2. **不要创建 `c_cpp_properties.json`**（EIDE 自动配置 C/C++ IntelliSense）
3. **Keil 项目独立存在**：EIDE 配置与 Keil 项目互不影响
4. **LVGL 源文件**：已在 eide.yml 中完整配置，无需额外操作

---

## 📊 与现有构建系统的兼容性

| 构建系统 | 状态 | 说明 |
|----------|------|------|
| **Keil uVision** | ✅ 保留 | `TOUCH.uvprojx` 不受影响 |
| **GCC Makefile** | ✅ 保留 | `Makefile` 可独立使用 |
| **CMake** | ✅ 保留 | `CMakeLists.txt` 不受影响 |
| **PlatformIO** | ✅ 保留 | `platformio.ini` 不受影响 |
| **EIDE** | ✅ 新增 | 使用 GCC 工具链编译 |

---

## 🔗 相关文件

- [eide.yml](Project/.eide/eide.yml) - 主配置文件
- [Makefile](Project/Makefile) - GCC Makefile
- [STM32F407VGTx_FLASH.ld](Project/STM32F407VGTx_FLASH.ld) - 链接脚本
- [TOUCH.code-workspace](Project/TOUCH.code-workspace) - VSCode 工作空间
