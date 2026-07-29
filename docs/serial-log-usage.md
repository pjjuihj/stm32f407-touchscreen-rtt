# 串口日志系统使用指南

## 概述

本项目实现了基于 USART1 的串口日志系统，支持多模块、多级别的日志记录和命令控制。

## 硬件连接

```
STM32F407ZG          USB串口模块
----------        ------------
PA9  (TX)  ---->  RX
PA10 (RX)  <----  TX
GND        ---->  GND
```

## 串口配置

- **波特率**: 115200
- **数据位**: 8
- **停止位**: 1
- **校验位**: 无

## 命令列表

### 基本命令

| 命令 | 功能 | 示例 |
|------|------|------|
| `HELP` | 显示帮助信息 | `HELP` |
| `STATUS` | 显示当前日志状态 | `STATUS` |
| `DUMP` | 导出所有日志 | `DUMP` |
| `CLEAR` | 清空日志缓冲区 | `CLEAR` |

### 模块控制命令

| 命令 | 功能 | 示例 |
|------|------|------|
| `TOUCH ON` | 开启触摸日志 | `TOUCH ON` |
| `TOUCH OFF` | 关闭触摸日志 | `TOUCH OFF` |
| `LCD ON` | 开启 LCD 日志 | `LCD ON` |
| `LCD OFF` | 关闭 LCD 日志 | `LCD OFF` |
| `SYSTEM ON` | 开启系统日志 | `SYSTEM ON` |
| `SYSTEM OFF` | 关闭系统日志 | `SYSTEM OFF` |

### 日志级别命令

| 命令 | 功能 | 级别说明 |
|------|------|---------|
| `LEVEL TOUCH 1` | 设置触摸日志级别 | 1=ERROR |
| `LEVEL TOUCH 2` | | 2=WARNING |
| `LEVEL TOUCH 3` | | 3=INFO |
| `LEVEL TOUCH 4` | | 4=DEBUG |
| `LEVEL LCD 1-4` | 设置 LCD 日志级别 | 同上 |
| `LEVEL SYSTEM 1-4` | 设置系统日志级别 | 同上 |

### 测试命令

| 命令 | 功能 |
|------|------|
| `TEST RUN` | 运行所有测试用例 |
| `TEST WRITE` | 测试日志写入 |
| `TEST OVERFLOW` | 测试缓冲区溢出 |
| `TEST CLEAR` | 清空测试日志 |

## 日志格式

```
[MODULE][TIMESTAMP][LEVEL] message
```

示例：
```
[SYSTEM][000000100ms][INFO] System starting...
[TOUCH][000001234ms][DEBUG] Touch: x=100 y=200
[LCD][000002345ms][WARNING] Screen refresh slow
```

## 快速开始

1. **连接硬件**
   - 将 STM32F407 的 PA9 (TX) 连接到 USB串口模块的 RX
   - 将 STM32F407 的 PA10 (RX) 连接到 USB串口模块的 TX
   - 连接 GND

2. **打开串口终端**
   - 打开串口终端软件（如 PuTTY、SecureCRT）
   - 设置波特率为 115200
   - 打开串口

3. **复位设备**
   - 按下 STM32 开发板上的 RESET 按钮
   - 串口终端应显示启动信息

4. **发送命令**
   - 输入 `HELP` 查看可用命令
   - 输入 `STATUS` 查看当前状态
   - 输入 `TOUCH ON` 开启触摸日志

## 测试

### 运行测试脚本

```bash
# 基本测试
python test_simple.py

# 完整测试
python test_final.py
```

### 运行设备测试

在串口终端发送：
```
TEST RUN
```

## 故障排除

### 问题：串口无输出

检查：
1. 硬件连接是否正确
2. 波特率是否设置为 115200
3. 设备是否已复位

### 问题：命令无响应

检查：
1. 命令是否正确输入
2. 是否按下了回车键
3. 发送速度是否太快（建议逐字符发送）

### 问题：日志不显示

检查：
1. 模块是否已启用（`STATUS` 命令）
2. 日志级别是否正确（`LEVEL` 命令）
3. 缓冲区是否已满（`DUMP` 命令）

## 技术规格

- **MCU**: STM32F407ZG
- **系统时钟**: 168MHz
- **USART**: USART1 (PA9/PA10)
- **波特率**: 115200
- **日志缓冲区**: 64 条
- **命令缓冲区**: 64 字节
- **支持模块**: TOUCH, LCD, SYSTEM
- **日志级别**: ERROR(1), WARNING(2), INFO(3), DEBUG(4)

## 文件结构

```
├── USER/
│   ├── USART/
│   │   ├── usart.h      // USART 驱动头文件
│   │   └── usart.c      // USART 驱动实现
│   └── LOG/
│       ├── log.h        // 日志系统头文件
│       └── log.c        // 日志系统实现
├── TEST/
│   ├── test_log.h       // 测试模块头文件
│   └── test_log.c       // 测试模块实现
├── Main/
│   └── main.c           // 主程序
├── test_simple.py       // 基本测试脚本
├── test_final.py        // 完整测试脚本
└── docs/
    └── serial-log-usage.md  // 本文件
```
