# SEGGER RTT 日志系统实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 集成 SEGGER RTT 到 STM32F407ZG 项目，替代 LVGL 日志回调的 UART 输出，解决回调函数阻塞导致的崩溃问题。

**Architecture:** 使用官方 SEGGER RTT 库，双通道分离（LVGL 日志通道 0，应用日志通道 1），保留 UART 命令功能。通过 pyOCD RTT 查看日志输出。

**Tech Stack:** SEGGER RTT, STM32F407ZG, CMSIS-DAP, pyOCD, Keil ARMCC, LVGL 9.5.0

## Global Constraints

- MCU: STM32F407ZG (168MHz, 1MB Flash, 192KB RAM)
- 调试器: CMSIS-DAP (支持 RTT)
- 编译器: Keil ARMCC V5.06
- LVGL 版本: v9.5.0
- 保留原有 UART 命令功能
- 保留原有日志代码（标记废弃）

---

## 文件结构

```
SEGGER_RTT/
├── RTT/
│   ├── SEGGER_RTT.c           # 核心实现（从官方库复制）
│   ├── SEGGER_RTT.h           # 核心头文件（从官方库复制）
│   └── SEGGER_RTT_Conf.h      # 配置文件（自定义）
└── SEGGER_RTT.h               # 公共头文件（自定义）

GUI/
└── gui_driver.c               # 修改：简化 LVGL 日志回调

USER/
└── LOG/
    └── log.c                  # 修改：添加 RTT 输出

Main/
└── main.c                     # 修改：添加 RTT 初始化

Project/
├── TOUCH.uvprojx              # 修改：添加 RTT 源文件和头文件路径
└── OBJ/TOUCH.sct              # 已包含 .rtt_block 段
```

---

## Task 1: 下载并集成官方 SEGGER RTT 库

**Files:**
- Create: `SEGGER_RTT/RTT/SEGGER_RTT.c`
- Create: `SEGGER_RTT/RTT/SEGGER_RTT.h`
- Create: `SEGGER_RTT/RTT/SEGGER_RTT_Conf.h`
- Create: `SEGGER_RTT/SEGGER_RTT.h`

**Interfaces:**
- Produces: `SEGGER_RTT_Init()`, `SEGGER_RTT_WriteString()`, `SEGGER_RTT_Write()`

- [ ] **Step 1: 创建 SEGGER_RTT 目录结构**

```bash
mkdir -p SEGGER_RTT/RTT
```

- [ ] **Step 2: 下载官方 SEGGER RTT 库**

从 https://www.segger.com/products/development-tools/rtt-viewer/source-code/ 下载源码，或使用以下简化实现：

- [ ] **Step 3: 创建 SEGGER_RTT_Conf.h**

```c
// SEGGER_RTT/RTT/SEGGER_RTT_Conf.h
#ifndef SEGGER_RTT_CONF_H
#define SEGGER_RTT_CONF_H

#define SEGGER_RTT_MAX_NUM_UP_BUFFERS     2
#define SEGGER_RTT_MAX_NUM_DOWN_BUFFERS   2
#define SEGGER_RTT_BUFFER_SIZE_UP         1024
#define SEGGER_RTT_BUFFER_SIZE_DOWN       16
#define SEGGER_RTT_MODE_NO_BLOCK_TRIM     1
#define SEGGER_RTT_LOCK()                 __disable_irq()
#define SEGGER_RTT_UNLOCK()               __enable_irq()

#endif
```

- [ ] **Step 4: 创建 SEGGER_RTT.h 公共头文件**

```c
// SEGGER_RTT/SEGGER_RTT.h
#ifndef SEGGER_RTT_H
#define SEGGER_RTT_H

#include "RTT/SEGGER_RTT.h"

#endif
```

- [ ] **Step 5: 验证编译**

```bash
# 使用 Keil 编译，确认 0 错误
```

- [ ] **Step 6: 提交**

```bash
git add SEGGER_RTT/
git commit -m "feat(rtt): add SEGGER RTT library files"
```

---

## Task 2: 配置 Keil 工程

**Files:**
- Modify: `Project/TOUCH.uvprojx`

**Interfaces:**
- Consumes: SEGGER_RTT 源文件
- Produces: Keil 工程可编译 RTT 代码

- [ ] **Step 1: 打开 Keil 工程**

```bash
# 打开 Project/TOUCH.uvprojx
```

- [ ] **Step 2: 添加 SEGGER_RTT 分组**

```
Project → Manage Project Items → 添加新分组 "SEGGER_RTT"
```

- [ ] **Step 3: 添加源文件**

```
将 SEGGER_RTT/RTT/SEGGER_RTT.c 添加到 SEGGER_RTT 分组
```

- [ ] **Step 4: 添加头文件路径**

```
Project → Options for Target → C/C++ → Include Paths
添加: ../SEGGER_RTT
添加: ../SEGGER_RTT/RTT
```

- [ ] **Step 5: 验证编译**

```bash
# 编译工程，确认 0 错误
```

- [ ] **Step 6: 提交**

```bash
git add Project/TOUCH.uvprojx
git commit -m "build(rtt): add SEGGER RTT to Keil project"
```

---

## Task 3: 实现 RTT 初始化

**Files:**
- Modify: `Main/main.c`

**Interfaces:**
- Consumes: `SEGGER_RTT_Init()`
- Produces: RTT 在 main() 中初始化

- [ ] **Step 1: 添加头文件**

```c
// Main/main.c 顶部添加
#include "SEGGER_RTT.h"
```

- [ ] **Step 2: 在 main() 中调用初始化**

```c
int main(void)
{
    HAL_Init();
    Stm32_Clock_Init(336,8,2,7);
    delay_init();
    LED_Init();
    BEEP_Init();
    KEY_Init();
    USART1_Init();
    delay_ms(100);
    
    // 初始化 RTT（必须在日志之前）
    SEGGER_RTT_Init();
    
    // 原有代码继续...
    HAL_UART_Receive_IT(&huart1, &rxChar, 1);
    Log_Init();
    // ...
}
```

- [ ] **Step 3: 编译验证**

```bash
# 编译工程，确认 0 错误
```

- [ ] **Step 4: 烧录测试**

```bash
pyocd flash -t stm32f407zg Project/OBJ/TOUCH.hex
pyocd reset -t stm32f407zg -m hw
```

- [ ] **Step 5: 验证 RTT 初始化**

```bash
# 终端运行
pyocd rtt -t stm32f407zg --up-channel-id 0 --timeout 3
# 预期：无输出（RTT 已初始化但无数据写入）
```

- [ ] **Step 6: 提交**

```bash
git add Main/main.c
git commit -m "feat(rtt): initialize RTT in main()"
```

---

## Task 4: 修改 LVGL 日志回调

**Files:**
- Modify: `GUI/gui_driver.c`

**Interfaces:**
- Consumes: `SEGGER_RTT_WriteString()`
- Produces: LVGL 日志输出到 RTT 通道 0

- [ ] **Step 1: 添加头文件**

```c
// GUI/gui_driver.c 顶部添加
#include "SEGGER_RTT.h"

#define RTT_CHANNEL_LVGL   0
```

- [ ] **Step 2: 简化 LVGL 日志回调**

```c
// 替换原有的 lv_log_print_g_cb 函数
static void lv_log_print_g_cb(lv_log_level_t level, const char *buf)
{
    // LVGL 已经格式化了完整的日志消息
    // 直接写入 RTT 通道 0，不添加任何前缀
    SEGGER_RTT_WriteString(RTT_CHANNEL_LVGL, buf);
    SEGGER_RTT_WriteString(RTT_CHANNEL_LVGL, "\n");
}
```

- [ ] **Step 3: 标记旧代码为废弃**

```c
// 在原有环形缓冲区代码前添加注释
// ============================================
// DEPRECATED: 以下代码已废弃，保留作为备份
// 新代码使用 SEGGER RTT 替代
// ============================================
```

- [ ] **Step 4: 编译验证**

```bash
# 编译工程，确认 0 错误
```

- [ ] **Step 5: 烧录测试**

```bash
pyocd flash -t stm32f407zg Project/OBJ/TOUCH.hex
pyocd reset -t stm32f407zg -m hw
```

- [ ] **Step 6: 验证 LVGL 日志输出**

```bash
# 终端运行
pyocd rtt -t stm32f407zg --up-channel-id 0 --timeout 5
# 预期：看到 LVGL 初始化日志
```

- [ ] **Step 7: 提交**

```bash
git add GUI/gui_driver.c
git commit -m "feat(rtt): redirect LVGL logs to RTT channel 0"
```

---

## Task 5: 修改应用日志

**Files:**
- Modify: `USER/LOG/log.c`

**Interfaces:**
- Consumes: `SEGGER_RTT_WriteString()`
- Produces: 应用日志输出到 RTT 通道 1

- [ ] **Step 1: 添加头文件**

```c
// USER/LOG/log.c 顶部添加
#include "SEGGER_RTT.h"

#define RTT_CHANNEL_APP  1
```

- [ ] **Step 2: 修改 Log_Write 函数**

```c
void Log_Write(LogModule module, LogLevel level, const char *fmt, ...)
{
    if (!g_logConfig.enabled[module]) return;
    if (level > g_logConfig.level[module]) return;

    char buf[128];
    va_list args;
    
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    
    char msg[160];
    snprintf(msg, sizeof(msg), "[%s][%s] %s\r\n", 
             LogModuleNames[level], LogLevelNames[level], buf);
    
    // 输出到 RTT 通道 1
    SEGGER_RTT_WriteString(RTT_CHANNEL_APP, msg);
    
    // 保留 UART 输出（可选，调试期间）
    // USART1_SendString(msg);
}
```

- [ ] **Step 3: 编译验证**

```bash
# 编译工程，确认 0 错误
```

- [ ] **Step 4: 烧录测试**

```bash
pyocd flash -t stm32f407zg Project/OBJ/TOUCH.hex
pyocd reset -t stm32f407zg -m hw
```

- [ ] **Step 5: 验证应用日志输出**

```bash
# 终端运行
pyocd rtt -t stm32f407zg --up-channel-id 1 --timeout 5
# 预期：看到 "[SYSTEM][INFO] System starting..." 等日志
```

- [ ] **Step 6: 提交**

```bash
git add USER/LOG/log.c
git commit -m "feat(rtt): redirect app logs to RTT channel 1"
```

---

## Task 6: 验证完整功能

**Files:**
- 无新增修改

**Interfaces:**
- Consumes: RTT 初始化、LVGL 日志、应用日志
- Produces: 完整功能验证

- [ ] **Step 1: 编译完整工程**

```bash
# Keil 编译，确认 0 错误 0 警告
```

- [ ] **Step 2: 烧录固件**

```bash
pyocd erase -t stm32f407zg --chip
pyocd flash -t stm32f407zg Project/OBJ/TOUCH.hex
pyocd reset -t stm32f407zg -m hw
```

- [ ] **Step 3: 验证 LVGL 日志**

```bash
pyocd rtt -t stm32f407zg --up-channel-id 0 --timeout 8
# 预期：看到 LVGL 初始化和运行日志
```

- [ ] **Step 4: 验证应用日志**

```bash
pyocd rtt -t stm32f407zg --up-channel-id 1 --timeout 3
# 预期：看到 "[SYSTEM][INFO] System starting..." 等日志
```

- [ ] **Step 5: 验证 UART 命令**

```bash
# 使用串口工具连接 COM3 (115200)
# 发送 STATUS 命令
# 预期：返回 "--- Log Status ---" 等信息
```

- [ ] **Step 6: 验证双通道同时工作**

```bash
# 终端 1：pyocd rtt -t stm32f407zg --up-channel-id 0
# 终端 2：pyocd rtt -t stm32f407zg --up-channel-id 1
# 终端 3：串口工具连接 COM3
# 操作：触摸屏幕、发送串口命令
# 预期：三个终端都有输出
```

- [ ] **Step 7: 提交**

```bash
git add -A
git commit -m "test(rtt): verify complete RTT logging functionality"
```

---

## Task 7: 文档更新

**Files:**
- Modify: `docs/serial-log-usage.md`

**Interfaces:**
- Consumes: RTT 使用方法
- Produces: 更新的使用文档

- [ ] **Step 1: 更新串口日志使用文档**

```markdown
# 串口日志使用文档

## RTT 日志输出

### 查看 LVGL 日志
```bash
pyocd rtt -t stm32f407zg --up-channel-id 0
```

### 查看应用日志
```bash
pyocd rtt -t stm32f407zg --up-channel-id 1
```

### 查看所有日志
```bash
pyocd rtt -t stm32f407zg
```

## UART 命令

保留原有串口命令功能：
- STATUS: 查看日志状态
- HELP: 显示帮助
- TEST RUN: 运行测试
```

- [ ] **Step 2: 提交**

```bash
git add docs/serial-log-usage.md
git commit -m "docs(rtt): update logging usage documentation"
```

---

## 验证清单

| 步骤 | 验证内容 | 预期结果 | 状态 |
|------|----------|----------|------|
| 1 | 编译 RTT 库 | 0 错误 | ☐ |
| 2 | 链接 RTT 段 | .rtt_block 在 RAM 中 | ☐ |
| 3 | 初始化 RTT | SEGGER_RTT_Init() 调用成功 | ☐ |
| 4 | 通道 0 写入 | pyOCD rtt 可见 LVGL 日志 | ☐ |
| 5 | 通道 1 写入 | pyOCD rtt 可见应用日志 | ☐ |
| 6 | UART 命令 | STATUS/HELP 仍正常 | ☐ |
| 7 | 双通道同时工作 | 三个终端都有输出 | ☐ |

---

## 回滚方案

如果 RTT 集成失败，可以快速回滚：

1. 恢复 `gui_driver.c` 中的 LVGL 日志回调
2. 恢复 `log.c` 中的 Log_Write 函数
3. 从 Keil 工程中移除 SEGGER_RTT 分组
4. 删除 `SEGGER_RTT/` 目录

```bash
git checkout HEAD~5 -- GUI/gui_driver.c USER/LOG/log.c Main/main.c
```

---

**计划版本:** v1.0
**创建日期:** 2026-07-31
**预计耗时:** 2-3 小时
