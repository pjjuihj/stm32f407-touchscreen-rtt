# LVGL DMA 日志系统实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 解决 LVGL 9.5 日志启用后 MCU 卡死问题，实现 DMA 非阻塞日志输出

**Architecture:** 分离缓冲区 + 共享 DMA 调度器。LVGL 日志和自定义日志分别写入独立环形缓冲区，DMA 调度器按优先级（LVGL > 自定义）从缓冲区读取数据并启动 DMA 传输。

**Tech Stack:** STM32F407 HAL, LVGL 9.5, UART1 DMA (DMA2 Stream7)

---

## 文件结构

| 文件 | 操作 | 职责 |
|------|------|------|
| `GUI/gui_driver.h` | 修改 | 新增 `gui_log_write()` 声明 |
| `GUI/gui_driver.c` | 修改 | 实现环形缓冲区、DMA 调度器、日志回调 |
| `USER/USART/usart.c` | 修改 | 添加 DMA 配置、中断处理 |
| `USER/USART/usart.h` | 修改 | 添加 DMA 句柄声明 |
| `USER/LOG/log.c` | 修改 | 修改 `Log_Write` 使用 `gui_log_write()` |
| `Main/main.c` | 修改 | 调整初始化顺序、主循环 |
| `lv_conf.h` | 修改 | 配置 LVGL 日志级别 |

---

## 全局约束

- MCU: STM32F407ZG, 168MHz
- LVGL 版本: 9.5
- UART: USART1, 115200 baud, PA9-TX, PA10-RX
- DMA: DMA2 Stream7, Channel 4
- 日志级别: LV_LOG_LEVEL_WARN
- 缓冲区大小: LVGL 1024 bytes, 自定义 1024 bytes, DMA 256 bytes

---

## Task 1: 添加 DMA 句柄声明

**Files:**
- Modify: `USER/USART/usart.h`

**Interfaces:**
- Produces: `extern DMA_HandleTypeDef hdma_usart1_tx`

- [ ] **Step 1: 在 usart.h 中添加 DMA 句柄声明**

在 `usart.h` 文件末尾，`#endif` 之前添加：

```c
/* DMA 句柄 */
extern DMA_HandleTypeDef hdma_usart1_tx;
```

- [ ] **Step 2: 验证编译通过**

Run: 编译项目
Expected: 编译成功，无错误

- [ ] **Step 3: Commit**

```bash
git add USER/USART/usart.h
git commit -m "feat(uart): add DMA handle declaration"
```

---

## Task 2: 配置 USART1 DMA

**Files:**
- Modify: `USER/USART/usart.c`

**Interfaces:**
- Consumes: `hdma_usart1_tx` (从 Task 1)
- Produces: DMA 初始化代码、中断处理函数

- [ ] **Step 1: 在 usart.c 中添加 DMA 句柄定义**

在文件开头，`#include` 之后添加：

```c
/* DMA 句柄定义 */
DMA_HandleTypeDef hdma_usart1_tx;
```

- [ ] **Step 2: 在 HAL_UART_MspInit 中添加 DMA 配置**

在 `HAL_UART_MspInit` 函数末尾，`}` 之前添加：

```c
    /* 配置 USART1 DMA */
    __HAL_RCC_DMA2_CLK_ENABLE();
    
    hdma_usart1_tx.Instance = DMA2_Stream7;
    hdma_usart1_tx.Init.Channel = DMA_CHANNEL_4;
    hdma_usart1_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_usart1_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_usart1_tx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_usart1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart1_tx.Init.Mode = DMA_NORMAL;
    hdma_usart1_tx.Init.Priority = DMA_PRIORITY_LOW;
    hdma_usart1_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    HAL_DMA_Init(&hdma_usart1_tx);
    
    __HAL_LINKDMA(huart, hdmatx, hdma_usart1_tx);
    
    /* 配置 DMA 中断 */
    HAL_NVIC_SetPriority(DMA2_Stream7_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream7_IRQn);
```

- [ ] **Step 3: 添加 DMA2 Stream7 中断处理函数**

在 `usart.c` 文件末尾添加：

```c
/**
 * @brief DMA2 Stream7 中断处理（USART1_TX）
 */
void DMA2_Stream7_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart1_tx);
}
```

- [ ] **Step 4: 验证编译通过**

Run: 编译项目
Expected: 编译成功，无错误

- [ ] **Step 5: Commit**

```bash
git add USER/USART/usart.c USER/USART/usart.h
git commit -m "feat(uart): configure USART1 DMA for non-blocking TX"
```

---

## Task 3: 实现环形缓冲区

**Files:**
- Modify: `GUI/gui_driver.c`

**Interfaces:**
- Produces: `ring_buf_t` 结构、`ring_buf_put()`、`ring_buf_get()`、`ring_buf_count()`

- [ ] **Step 1: 在 gui_driver.c 中添加环形缓冲区结构定义**

在文件开头，`#include` 之后添加：

```c
/*===========================================================================
 * 环形缓冲区结构
 *===========================================================================*/

/**
 * @brief 通用环形缓冲区结构
 */
typedef struct {
    volatile uint16_t head;     // 写入位置
    volatile uint16_t tail;     // 读取位置
    volatile uint16_t count;    // 当前数据量
    uint16_t size;              // 缓冲区总大小
    char *buf;                  // 数据存储区
} ring_buf_t;
```

- [ ] **Step 2: 实现环形缓冲区操作函数**

在结构定义之后添加：

```c
/**
 * @brief 写入单个字符到环形缓冲区（中断安全）
 */
static void ring_buf_put(ring_buf_t *ring, char c)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    
    if(ring->count < ring->size) {
        ring->buf[ring->head] = c;
        ring->head = (ring->head + 1) % ring->size;
        ring->count++;
    }
    // 缓冲区满时丢弃字符
    
    __set_PRIMASK(primask);
}

/**
 * @brief 从环形缓冲区读取单个字符
 * @return 读取的字符，缓冲区空返回 -1
 */
static int ring_buf_get(ring_buf_t *ring)
{
    if(ring->count == 0) return -1;
    
    char c = ring->buf[ring->tail];
    ring->tail = (ring->tail + 1) % ring->size;
    ring->count--;
    return c;
}

/**
 * @brief 获取环形缓冲区中的数据量
 */
static uint16_t ring_buf_count(ring_buf_t *ring)
{
    return ring->count;
}
```

- [ ] **Step 3: 验证编译通过**

Run: 编译项目
Expected: 编译成功，无错误

- [ ] **Step 4: Commit**

```bash
git add GUI/gui_driver.c
git commit -m "feat(log): implement ring buffer with interrupt safety"
```

---

## Task 4: 实现 DMA 调度器

**Files:**
- Modify: `GUI/gui_driver.c`

**Interfaces:**
- Consumes: `ring_buf_t`、`ring_buf_put()`、`ring_buf_get()`、`ring_buf_count()` (从 Task 3)
- Produces: `dma_scheduler_t`、`dma_scheduler_run()`、`lvgl_ring`、`custom_ring`

- [ ] **Step 1: 添加 DMA 调度器结构和状态定义**

在环形缓冲区函数之后添加：

```c
/*===========================================================================
 * DMA 调度器
 *===========================================================================*/

/**
 * @brief DMA 发送状态
 */
typedef enum {
    DMA_IDLE = 0,               // 空闲，可启动新传输
    DMA_TX_LVGL,                // 正在发送 LVGL 日志
    DMA_TX_CUSTOM,              // 正在发送自定义日志
} dma_state_t;

/**
 * @brief DMA 调度器上下文
 */
typedef struct {
    dma_state_t state;          // 当前状态
    uint8_t tx_buf[256];        // DMA 发送缓冲区
    uint16_t tx_len;            // 当前发送长度
} dma_scheduler_t;

// LVGL 日志缓冲区
static char lvgl_log_buf[1024];
static ring_buf_t lvgl_ring = {
    .head = 0, .tail = 0, .count = 0,
    .size = 1024, .buf = lvgl_log_buf
};

// 自定义日志缓冲区
static char custom_log_buf[1024];
static ring_buf_t custom_ring = {
    .head = 0, .tail = 0, .count = 0,
    .size = 1024, .buf = custom_log_buf
};

// DMA 调度器
static dma_scheduler_t dma_scheduler = {
    .state = DMA_IDLE,
    .tx_len = 0
};
```

- [ ] **Step 2: 实现 DMA 调度器函数**

在全局变量之后添加：

```c
/**
 * @brief 从环形缓冲区填充 DMA 发送缓冲区
 * @return 实际填充的字节数
 */
static uint16_t fill_dma_buf(ring_buf_t *ring, uint16_t max_len)
{
    uint16_t len = 0;
    int c;
    
    while(len < max_len && (c = ring_buf_get(ring)) != -1) {
        dma_scheduler.tx_buf[len++] = (uint8_t)c;
    }
    
    return len;
}

/**
 * @brief DMA 调度器 - 主循环调用
 */
static void dma_scheduler_run(void)
{
    // 如果 DMA 正忙，直接返回
    if(dma_scheduler.state != DMA_IDLE) return;
    
    // 优先检查 LVGL 缓冲区
    if(ring_buf_count(&lvgl_ring) > 0) {
        dma_scheduler.tx_len = fill_dma_buf(&lvgl_ring, sizeof(dma_scheduler.tx_buf));
        dma_scheduler.state = DMA_TX_LVGL;
        HAL_UART_Transmit_DMA(&huart1, dma_scheduler.tx_buf, dma_scheduler.tx_len);
        return;
    }
    
    // LVGL 无数据，检查自定义缓冲区
    if(ring_buf_count(&custom_ring) > 0) {
        dma_scheduler.tx_len = fill_dma_buf(&custom_ring, sizeof(dma_scheduler.tx_buf));
        dma_scheduler.state = DMA_TX_CUSTOM;
        HAL_UART_Transmit_DMA(&huart1, dma_scheduler.tx_buf, dma_scheduler.tx_len);
        return;
    }
}
```

- [ ] **Step 3: 验证编译通过**

Run: 编译项目
Expected: 编译成功，无错误

- [ ] **Step 4: Commit**

```bash
git add GUI/gui_driver.c
git commit -m "feat(log): implement DMA scheduler with priority"
```

---

## Task 5: 实现 LVGL 日志回调

**Files:**
- Modify: `GUI/gui_driver.c`

**Interfaces:**
- Consumes: `ring_buf_put()`、`lvgl_ring` (从 Task 4)
- Produces: `lv_log_print_g_cb()`

- [ ] **Step 1: 实现 LVGL 日志回调函数**

在 DMA 调度器函数之后添加：

```c
/*===========================================================================
 * LVGL 日志回调
 *===========================================================================*/

/**
 * @brief LVGL 9.5 日志回调 - 仅写入 LVGL 缓冲区
 */
static void lv_log_print_g_cb(lv_log_level_t level, const char *buf)
{
    // 递归保护
    static volatile bool in_log_cb = false;
    if(in_log_cb) return;
    in_log_cb = true;
    
    // 日志级别前缀
    const char *prefix;
    switch(level) {
        case LV_LOG_LEVEL_TRACE: prefix = "[T] "; break;
        case LV_LOG_LEVEL_INFO:  prefix = "[I] "; break;
        case LV_LOG_LEVEL_WARN:  prefix = "[W] "; break;
        case LV_LOG_LEVEL_ERROR: prefix = "[E] "; break;
        case LV_LOG_LEVEL_USER:  prefix = "[U] "; break;
        default:                 prefix = "[?] "; break;
    }
    
    // 写入前缀
    for(const char *p = prefix; *p; p++) {
        ring_buf_put(&lvgl_ring, *p);
    }
    
    // 写入日志内容
    for(const char *p = buf; *p; p++) {
        ring_buf_put(&lvgl_ring, *p);
    }
    
    // 换行符
    ring_buf_put(&lvgl_ring, '\r');
    ring_buf_put(&lvgl_ring, '\n');
    
    in_log_cb = false;
}
```

- [ ] **Step 2: 验证编译通过**

Run: 编译项目
Expected: 编译成功，无错误

- [ ] **Step 3: Commit**

```bash
git add GUI/gui_driver.c
git commit -m "feat(log): implement LVGL log callback with recursion guard"
```

---

## Task 6: 实现公共接口

**Files:**
- Modify: `GUI/gui_driver.c`
- Modify: `GUI/gui_driver.h`

**Interfaces:**
- Consumes: `lv_log_print_g_cb()`、`dma_scheduler_run()`、`custom_ring` (从 Task 4, 5)
- Produces: `gui_log_init()`、`gui_log_flush()`、`gui_log_write()`

- [ ] **Step 1: 实现 gui_log_init()**

在 LVGL 日志回调函数之后添加：

```c
/*===========================================================================
 * 公共接口
 *===========================================================================*/

/**
 * @brief 初始化日志系统（必须在 lv_init 之前调用）
 */
void gui_log_init(void)
{
    // 清空缓冲区
    memset(&lvgl_ring, 0, sizeof(lvgl_ring));
    memset(&custom_ring, 0, sizeof(custom_ring));
    memset(&dma_scheduler, 0, sizeof(dma_scheduler));
    
    // 注册 LVGL 日志回调
    lv_log_register_print_cb(lv_log_print_g_cb);
}
```

- [ ] **Step 2: 实现 gui_log_flush()**

在 `gui_log_init()` 之后添加：

```c
/**
 * @brief 主循环调用 - 调度 DMA 发送
 */
void gui_log_flush(void)
{
    dma_scheduler_run();
}
```

- [ ] **Step 3: 实现 gui_log_write()**

在 `gui_log_flush()` 之后添加：

```c
/**
 * @brief 写入自定义日志
 */
void gui_log_write(const char *str)
{
    for(const char *p = str; *p; p++) {
        ring_buf_put(&custom_ring, *p);
    }
}
```

- [ ] **Step 4: 在 gui_driver.h 中添加函数声明**

在 `gui_driver.h` 文件末尾，`#endif` 之前添加：

```c
/**
 * @brief 写入自定义日志（供 Log_Write 调用）
 */
void gui_log_write(const char *str);
```

- [ ] **Step 5: 验证编译通过**

Run: 编译项目
Expected: 编译成功，无错误

- [ ] **Step 6: Commit**

```bash
git add GUI/gui_driver.c GUI/gui_driver.h
git commit -m "feat(log): implement public API for DMA log system"
```

---

## Task 7: 删除旧日志实现

**Files:**
- Modify: `GUI/gui_driver.c`

**Interfaces:**
- 消费: 旧的 `lv_log_print_g_cb`、`gui_log_flush` 空函数
- 产出: 清理后的代码

- [ ] **Step 1: 删除旧的 LVGL 日志回调函数**

删除以下代码块（约第 144-183 行）：

```c
#if LV_USE_LOG
#define LV_LOG_BUF_SIZE 1024
static char lv_log_buf[LV_LOG_BUF_SIZE];
static volatile uint16_t lv_log_head = 0;
static volatile uint16_t lv_log_tail = 0;

static void lv_log_put_char(char c)
{
    uint16_t next = (lv_log_head + 1) % LV_LOG_BUF_SIZE;
    if(next != lv_log_tail) {
        lv_log_buf[lv_log_head] = c;
        lv_log_head = next;
    }
}

/**
 * @brief LVGL日志回调 - 仅写入缓冲区，不访问UART（安全）
 */
static void lv_log_print_g_cb(lv_log_level_t level, const char *buf)
{
    const char *prefix;
    switch(level) {
        case LV_LOG_LEVEL_TRACE: prefix = "[TRACE] "; break;
        case LV_LOG_LEVEL_INFO:  prefix = "[INFO]  "; break;
        case LV_LOG_LEVEL_WARN:  prefix = "[WARN]  "; break;
        case LV_LOG_LEVEL_ERROR: prefix = "[ERROR] "; break;
        case LV_LOG_LEVEL_USER:  prefix = "[USER]  "; break;
        default:                 prefix = "[???]   "; break;
    }
    for(const char *p = prefix; *p; p++) lv_log_put_char(*p);
    for(const char *p = buf; *p; p++) lv_log_put_char(*p);
    lv_log_put_char('\r');
    lv_log_put_char('\n');
}
#endif
```

- [ ] **Step 2: 删除旧的 gui_log_init() 和 gui_log_flush()**

删除以下代码块（约第 186-208 行）：

```c
/**
 * @brief 初始化日志功能（必须在lv_init之前调用）
 */
void gui_log_init(void)
{
#if LV_USE_LOG
    lv_log_head = 0;
    lv_log_tail = 0;
    lv_log_register_print_cb(lv_log_print_g_cb);
#endif
}

/**
 * @brief 主循环调用 - 从缓冲区发送到UART
 */
void gui_log_flush(void)
{
#if LV_USE_LOG
    while(lv_log_tail != lv_log_head) {
        USART1_SendChar(lv_log_buf[lv_log_tail]);
        lv_log_tail = (lv_log_tail + 1) % LV_LOG_BUF_SIZE;
    }
#endif
}
```

- [ ] **Step 3: 验证编译通过**

Run: 编译项目
Expected: 编译成功，无错误

- [ ] **Step 4: Commit**

```bash
git add GUI/gui_driver.c
git commit -m "refactor(log): remove old log implementation"
```

---

## Task 8: 修改 Log_Write 使用新系统

**Files:**
- Modify: `USER/LOG/log.c`

**Interfaces:**
- 消费: `gui_log_write()` (从 Task 6)
- 产出: 修改后的 `Log_Write()`

- [ ] **Step 1: 在 log.c 中包含 gui_driver.h**

在 `log.c` 文件开头，`#include` 区域添加：

```c
#include "gui_driver.h"
```

- [ ] **Step 2: 修改 Log_Write 函数**

找到 `Log_Write` 函数中的 `USART1_SendString(log_str);` 行，替换为：

```c
    gui_log_write(log_str);
```

- [ ] **Step 3: 验证编译通过**

Run: 编译项目
Expected: 编译成功，无错误

- [ ] **Step 4: Commit**

```bash
git add USER/LOG/log.c
git commit -m "refactor(log): use gui_log_write in Log_Write"
```

---

## Task 9: 修改 main.c 初始化顺序

**Files:**
- Modify: `Main/main.c`

**Interfaces:**
- 消费: `gui_log_init()`、`gui_log_flush()` (从 Task 6)
- 产出: 正确的初始化顺序

- [ ] **Step 1: 调整初始化顺序**

在 `main()` 函数中，确保初始化顺序为：

```c
    // 1. USART1 初始化（包含 DMA 配置）
    USART1_Init();
    
    // 2. 延时等待 USART 稳定
    delay_ms(100);
    
    // 3. 启用 UART 接收中断
    HAL_UART_Receive_IT(&huart1, &rxChar, 1);
    
    // 4. 初始化自定义日志系统
    Log_Init();
    Log_Write(SYSTEM, INFO, "System starting...");
    
    // 5. 初始化 LCD
    LCD_Init();
    
    // 6. 初始化触摸屏
    Touch_Init();
    
    // 7. LVGL 日志初始化（必须在 lv_init 之前）
    gui_log_init();
    
    // 8. LVGL 初始化
    lv_init();
    
    // 9. 初始化 LVGL 时钟
    gui_tick_init();
    
    // 10. 初始化 LVGL 显示驱动
    gui_disp_init();
    
    // 11. 初始化 LVGL 触摸驱动
    gui_touch_init();
```

- [ ] **Step 2: 在主循环中调用 gui_log_flush()**

在 `while(1)` 循环中，确保有以下调用：

```c
    while(1)
    {
        // ... 其他处理 ...
        
        gui_log_flush();  // 调度 DMA 发送
        lv_task_handler();
        delay_ms(5);
    }
```

- [ ] **Step 3: 验证编译通过**

Run: 编译项目
Expected: 编译成功，无错误

- [ ] **Step 4: Commit**

```bash
git add Main/main.c
git commit -m "feat(log): update init sequence and main loop"
```

---

## Task 10: 配置 LVGL 日志级别

**Files:**
- Modify: `lv_conf.h` (根目录)

**Interfaces:**
- 消费: LVGL 9.5 日志配置宏
- 产出: 正确的日志配置

- [ ] **Step 1: 修改日志配置**

找到日志配置部分（约第 38-43 行），修改为：

```c
/*===========================================================================
 * 日志配置
 *===========================================================================*/
#define LV_USE_LOG             1
#if LV_USE_LOG
    #define LV_LOG_LEVEL        LV_LOG_LEVEL_WARN  // WARN 级别
    #define LV_LOG_PRINTF       0     // 使用自定义回调
#endif
```

- [ ] **Step 2: 验证编译通过**

Run: 编译项目
Expected: 编译成功，无错误

- [ ] **Step 3: Commit**

```bash
git add lv_conf.h
git commit -m "config(log): set LVGL log level to WARN"
```

---

## Task 11: 添加 DMA 错误处理

**Files:**
- Modify: `GUI/gui_driver.c`

**Interfaces:**
- 消费: `dma_scheduler` (从 Task 4)
- 产出: `HAL_UART_ErrorCallback()`

- [ ] **Step 1: 实现 UART 错误回调**

在 `gui_log_write()` 函数之后添加：

```c
/**
 * @brief UART DMA 发送完成回调
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance != USART1) return;
    
    dma_scheduler.state = DMA_IDLE;
    dma_scheduler.tx_len = 0;
    
    // 立即检查是否有更多数据要发送
    dma_scheduler_run();
}

/**
 * @brief UART 错误回调
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance != USART1) return;
    
    // 清除错误标志
    __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF | UART_CLEAR_NEF | 
                          UART_CLEAR_PEF | UART_CLEAR_FEF);
    
    // 重置 DMA 状态
    dma_scheduler.state = DMA_IDLE;
    dma_scheduler.tx_len = 0;
}
```

- [ ] **Step 2: 验证编译通过**

Run: 编译项目
Expected: 编译成功，无错误

- [ ] **Step 3: Commit**

```bash
git add GUI/gui_driver.c
git commit -m "feat(log): add DMA completion and error callbacks"
```

---

## Task 12: 硬件验证

**Files:**
- 无代码修改

**Interfaces:**
- 无

- [ ] **Step 1: 烧录固件**

将编译后的固件烧录到 STM32F407 开发板

- [ ] **Step 2: 连接串口调试助手**

- 连接 USB 转串口模块到 PA9 (TX)
- 打开串口调试助手，设置 115200 baud, 8N1

- [ ] **Step 3: 验证日志输出**

预期输出：
```
[W] System starting...
[W] LVGL initialized
... 其他 LVGL 日志 ...
```

- [ ] **Step 4: 验证无卡死**

- 观察 LVGL 界面是否正常刷新
- 确认触摸操作响应正常
- 确认日志持续输出，无中断

- [ ] **Step 5: 验证自定义日志**

- 在串口发送 `STATUS` 命令
- 确认返回系统状态信息

---

## 完成

所有任务完成后，LVGL DMA 日志系统将实现：

1. ✅ LVGL 日志通过 DMA 非阻塞输出
2. ✅ 自定义日志通过 DMA 非阻塞输出
3. ✅ LVGL 日志优先于自定义日志
4. ✅ 缓冲区满时静默丢弃字符
5. ✅ 递归保护防止死锁
6. ✅ DMA 错误处理

系统将不再因 LVGL 日志而卡死。
