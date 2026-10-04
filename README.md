# STM32F407 触摸屏 + LVGL + SEGGER RTT 日志系统

基于 STM32F407ZG 的触摸屏开发板项目，集成 LVGL v9.5.0 图形库和 SEGGER RTT 实时日志系统。

## 功能特性

- 🖥️ **LVGL GUI** - 支持按钮、标签、滑动等控件
- 📱 **触摸屏** - 支持电阻触摸 (XPT2046) 和电容触摸 (FT5426)
- 📊 **RTT 日志** - 实时日志输出，支持 RTT/UART 模式切换
- 🔧 **SystemView** - 支持 SEGGER SystemView 实时分析
- 📟 **串口命令** - 支持 STATUS/HELP/TEST 等命令

## 硬件配置

| 组件 | 型号 | 接口 |
|------|------|------|
| MCU | STM32F407ZGT6 | - |
| LCD | ILI9341 (240×320) | FSMC |
| 电阻触摸 | XPT2046 | 软件 SPI |
| 电容触摸 | FT5426 | 软件 I2C |
| 调试器 | CMSIS-DAP | SWD |

## RTT 日志系统

### 架构

```
┌─────────────────────────────────────────────────┐
│  LVGL 日志  │  应用日志  │  UART 命令  │
└──────┬──────┴──────┬──────┴──────┬──────┘
       ▼             ▼             ▼
  RTT Ch0       RTT Ch1        USART1
  (LVGL)        (App)          (命令)
```

### 通道配置

| 通道 | 名称 | 用途 | 缓冲区 |
|------|------|------|--------|
| 0 | LVGL_LOG | LVGL 日志 | 1024 字节 |
| 1 | App | 应用日志 | 512 字节 |
| 2 | SystemView | 实时分析 | 256 字节 |

### 模式切换

通过串口命令切换日志输出模式：

```
LOGMODE RTT     # 切换到 RTT 模式
LOGMODE UART    # 切换到 UART 模式 (默认)
```

## 快速开始

### 1. 编译

```bash
# 使用 Keil MDK
# 打开 Project/TOUCH.uvprojx
# 编译工程

# 或使用 EIDE MCP
eide_build
```

### 2. 烧录

```bash
# 使用 pyOCD
pyocd flash -t stm32f407zg Project/OBJ/TOUCH.hex

# 或使用 STM32CubeProgrammer
STM32_Programmer_CLI -c port=SWD -w Project/OBJ/TOUCH.hex -v -rst
```

### 3. 查看日志

**UART 模式（默认）：**
```bash
# 打开串口终端 (COM3, 115200)
# 复位开发板，立即看到日志
```

**RTT 模式：**
```bash
# 切换到 RTT 模式
LOGMODE RTT

# 查看日志
pyocd rtt -t stm32f407zg -a 0x20014f10
```

**SystemView：**
```bash
# 查看 SystemView 数据
pyocd rtt -t stm32f407zg -a 0x20014f10 --up-channel-id 2
```

## 串口命令

| 命令 | 说明 |
|------|------|
| `STATUS` | 显示日志状态 |
| `HELP` | 显示帮助信息 |
| `LOGMODE RTT` | 切换到 RTT 模式 |
| `LOGMODE UART` | 切换到 UART 模式 |
| `TOUCH ON/OFF` | 启用/禁用触摸日志 |
| `LCD ON/OFF` | 启用/禁用 LCD 日志 |
| `SYSTEM ON/OFF` | 启用/禁用系统日志 |
| `LEVEL <MOD> <1-4>` | 设置日志级别 |
| `DUMP` | 导出日志缓冲区 |
| `CLEAR` | 清空日志缓冲区 |
| `TEST RUN` | 运行测试 |
| `TEST WRITE` | 测试日志写入 |

## 文件结构

```
触摸屏_usart/
├── Main/               # 主程序
│   └── main.c
├── GUI/                # LVGL 驱动适配
│   ├── gui_driver.c    # 显示/触摸/日志驱动
│   └── gui_driver.h
├── USER/               # 外设驱动
│   ├── LCD/            # LCD 驱动
│   ├── TOUCH/          # 触摸驱动
│   ├── USART/          # 串口驱动
│   └── LOG/            # 日志系统
├── SEGGER_RTT/         # RTT 库
│   ├── RTT/            # 核心实现
│   └── Sys/            # SystemView
├── LVGL/               # LVGL 库
├── STM32F4xx_HAL_Driver/  # HAL 库
├── Project/            # Keil 工程
└── docs/               # 文档
```

## 设计文档

- [串口日志系统设计](docs/superpowers/specs/2026-07-28-serial-logging-system-design.md)
- [LVGL 移植设计](docs/superpowers/specs/2026-07-28-lvgl-porting-design.md)
- [RTT 日志系统设计](docs/superpowers/specs/2026-07-31-rtt-logging-design.md)

## 依赖

- Keil MDK-ARM 5.06+
- Python 3.8+
- pyOCD (烧录)
- CMSIS-DAP 调试器

## 项目治理与交付

项目需求和问题统一通过 GitHub Issues 管理，代码通过 Pull Request 评审；构建、自动化测试、实机验证、稳定版本发布和恢复按治理手册记录。

- [项目治理手册与模板](docs/project-management/README.md)

## 许可证

MIT License

## 作者

普吉 (pjjuihj)
