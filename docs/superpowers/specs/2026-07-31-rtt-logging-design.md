# SEGGER RTT 日志系统设计文档

## 1. 项目概述

### 1.1 项目目标

集成 SEGGER RTT (Real-Time Transfer) 到 STM32F407ZG 触摸屏项目，替代 LVGL 日志回调的 UART 输出，解决回调函数阻塞导致的崩溃问题。

### 1.2 问题背景

**当前问题：**
- LVGL 日志回调 `lv_log_print_g_cb()` 直接调用 `USART1_SendString()` 导致阻塞
- 阻塞发生在 LVGL 内部临界区，造成 HardFault 或死锁
- 尝试环形缓冲区 + DMA 方案仍然崩溃

**根本原因：**
- LVGL 日志回调在内部代码中被调用，可能处于中断或临界区
- `HAL_UART_Transmit()` 是阻塞操作，不能在回调中使用

### 1.3 解决方案

使用 SEGGER RTT 替代 UART 输出：
- RTT 写入 RAM 缓冲区，**零阻塞**
- 通过 SWD 调试器读取 RAM，不需要额外硬件
- 完美解决回调崩溃问题

## 2. 需求总结

| 需求 | 说明 |
|------|------|
| **RTT 用途** | 所有日志输出（LVGL + 应用） |
| **UART 保留** | 保留串口命令功能（STATUS/HELP/TEST） |
| **RTT 库** | 官方完整 SEGGER RTT 库 |
| **通道配置** | 双通道分离（LVGL 和应用日志） |
| **查看工具** | pyOCD RTT |

## 3. 架构设计

### 3.1 系统架构

```
┌─────────────────────────────────────────────────────────┐
│                    应用层                                │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐     │
│  │  LVGL 日志  │  │  应用日志   │  │  UART 命令  │     │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘     │
├─────────┼────────────────┼────────────────┼─────────────┤
│    ┌────▼────┐      ┌────▼────┐      ┌────▼────┐       │
│    │ RTT Ch0 │      │ RTT Ch1 │      │  USART1 │       │
│    │ (LVGL)  │      │  (App)  │      │ (命令)  │       │
│    └────┬────┘      └────┬────┘      └────┬────┘       │
├─────────┼────────────────┼────────────────┼─────────────┤
│    ┌────▼────────────────▼────┐      ┌────▼────┐       │
│    │    SEGGER RTT 库         │      │  HAL    │       │
│    │  (官方完整实现)          │      │  UART   │       │
│    └────────────┬─────────────┘      └────┬────┘       │
├─────────────────┼────────────────────────┼─────────────┤
│            ┌────▼────┐              ┌────▼────┐        │
│            │  RAM    │              │  DMA    │        │
│            │ 缓冲区  │              │  TX     │        │
│            └────┬────┘              └────┬────┘        │
├─────────────────┼────────────────────────┼─────────────┤
│            ┌────▼────┐              ┌────▼────┐        │
│            │ CMSIS-  │              │ USART1  │        │
│            │ DAP     │              │ PA9/PA10│        │
│            └─────────┘              └─────────┘        │
└─────────────────────────────────────────────────────────┘
```

### 3.2 模块职责

| 模块 | 职责 | 依赖 |
|------|------|------|
| **SEGGER RTT 库** | 管理 RAM 环形缓冲区 | 无 |
| **LVGL 日志回调** | 格式化 LVGL 日志 → RTT Ch0 | RTT 库 |
| **应用日志** | 格式化应用日志 → RTT Ch1 | RTT 库 |
| **UART 命令** | 处理串口命令 | HAL UART |

## 4. 通道配置

### 4.1 通道分配

| 通道 | 名称 | 用途 | 缓冲区大小 |
|------|------|------|-----------|
| 0 | LVGL_LOG | LVGL 日志输出 | 1024 字节 |
| 1 | APP_LOG | 应用日志输出 | 512 字节 |

### 4.2 RTT 配置 (SEGGER_RTT_Conf.h)

```c
#define SEGGER_RTT_MAX_NUM_UP_BUFFERS     2
#define SEGGER_RTT_MAX_NUM_DOWN_BUFFERS   2
#define SEGGER_RTT_BUFFER_SIZE_UP         1024
#define SEGGER_RTT_BUFFER_SIZE_DOWN       16
#define SEGGER_RTT_MODE_NO_BLOCK_TRIM     1
#define SEGGER_RTT_LOCK()                 __disable_irq()
#define SEGGER_RTT_UNLOCK()               __enable_irq()
```

## 5. 代码实现

### 5.1 LVGL 日志回调

```c
// gui_driver.c
#include "SEGGER_RTT.h"

#define RTT_CHANNEL_LVGL   0

static void lv_log_print_g_cb(lv_log_level_t level, const char *buf)
{
    // LVGL 已经格式化了完整的日志消息
    // 直接写入 RTT 通道 0，不添加任何前缀
    SEGGER_RTT_WriteString(RTT_CHANNEL_LVGL, buf);
    SEGGER_RTT_WriteString(RTT_CHANNEL_LVGL, "\n");
}

void gui_log_init(void)
{
    lv_log_register_print_cb(lv_log_print_g_cb);
}
```

### 5.2 应用日志集成

```c
// log.c
#include "SEGGER_RTT.h"

#define RTT_CHANNEL_APP  1

void Log_Write(LogModule module, LogLevel level, const char *fmt, ...)
{
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
}
```

### 5.3 初始化流程

```c
// main.c
#include "SEGGER_RTT.h"

int main(void)
{
    HAL_Init();
    Stm32_Clock_Init(336,8,2,7);
    delay_init();
    LED_Init();
    USART1_Init();
    
    // 初始化 RTT（必须在日志之前）
    SEGGER_RTT_Init();
    
    // 初始化日志系统
    Log_Init();
    gui_log_init();
    
    // 初始化 LVGL
    lv_init();
    gui_tick_init();
    gui_disp_init();
    gui_touch_init();
    
    while(1)
    {
        lv_task_handler();
        delay_ms(5);
    }
}
```

## 6. 构建系统集成

### 6.1 Keil 工程

**添加分组：**
```
SEGGER_RTT/
├── SEGGER_RTT.c
└── SEGGER_RTT.h
```

**头文件路径：**
```
../SEGGER_RTT
../SEGGER_RTT/RTT
```

**散列文件修改：**
```
RW_IRAM1 0x20000000 0x00020000  {
    .ANY (+RW +ZI)
    *(.rtt_block)
}
```

### 6.2 CMake

```cmake
set(SEGGER_RTT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/SEGGER_RTT)

target_sources(lvgl_pc_sim PRIVATE
    ${SEGGER_RTT_DIR}/RTT/SEGGER_RTT.c
)

target_include_directories(lvgl_pc_sim PRIVATE
    ${SEGGER_RTT_DIR}
    ${SEGGER_RTT_DIR}/RTT
)
```

## 7. 数据流

```
LVGL 内部代码
    │
    ▼
LV_LOG_TRACE/INFO/WARN/ERROR
    │
    ▼
lv_log_print_g_cb()
    │
    ├──► SEGGER_RTT_WriteString(0, buf)
    │         │
    │         ▼
    │    RAM 缓冲区 [1024 bytes]
    │         │
    │         ▼
    │    pyOCD RTT 读取
    │         │
    │         ▼
    │    PC 终端显示
    │
应用代码 (Log_Write)
    │
    ▼
SEGGER_RTT_WriteString(1, msg)
    │
    ▼
RAM 缓冲区 [512 bytes]
    │
    ▼
pyOCD RTT 读取 → PC 终端显示

UART 命令 (STATUS/HELP/TEST)
    │
    ▼
USART1_SendString()  ← 保持不变
    │
    ▼
COM3 串口终端
```

## 8. 验证清单

| 步骤 | 验证内容 | 预期结果 |
|------|----------|----------|
| 1 | 编译 RTT 库 | 0 错误 |
| 2 | 链接 RTT 段 | .rtt_block 在 RAM 中 |
| 3 | 初始化 RTT | SEGGER_RTT_Init() 调用成功 |
| 4 | 通道 0 写入 | pyOCD rtt 可见 LVGL 日志 |
| 5 | 通道 1 写入 | pyOCD rtt 可见应用日志 |
| 6 | UART 命令 | STATUS/HELP 仍正常 |

## 9. 使用方法

### 9.1 查看 RTT 输出

```bash
# 查看所有通道
pyocd rtt -t stm32f407zg

# 只查看 LVGL 日志
pyocd rtt -t stm32f407zg --up-channel-id 0

# 只查看应用日志
pyocd rtt -t stm32f407zg --up-channel-id 1
```

### 9.2 串口命令

```bash
# 保留原有命令
STATUS      # 查看日志状态
HELP        # 显示帮助
TEST RUN    # 运行测试
```

## 10. 优势对比

| 特性 | UART | RTT |
|------|------|-----|
| 阻塞 | ✅ 会阻塞 | ❌ 零阻塞 |
| 占用引脚 | PA9/PA10 | 无（用 SWD） |
| 速度 | 115200 baud | 数 MB/s |
| 调试器需求 | 无 | CMSIS-DAP |
| 回调安全 | ❌ 不安全 | ✅ 安全 |

## 11. 文件结构

```
SEGGER_RTT/
├── RTT/
│   ├── SEGGER_RTT.c
│   ├── SEGGER_RTT.h
│   └── SEGGER_RTT_Conf.h
└── Sys/
    └── (可选) SEGGER_SYSVIEW.c

GUI/
└── gui_driver.c  (修改日志回调)

USER/
└── LOG/
    └── log.c  (修改应用日志)

Main/
└── main.c  (添加 RTT 初始化)
```

## 12. 风险与缓解

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| RAM 不足 | 系统崩溃 | RTT 缓冲区仅 1.5KB，影响小 |
| RTT 段未链接 | 无输出 | 验证 .rtt_block 在 RAM 中 |
| pyOCD 不支持 | 无法查看 | 已验证 CMSIS-DAP 支持 RTT |

---

**文档版本:** v1.0
**创建日期:** 2026-07-31
**作者:** Claude
