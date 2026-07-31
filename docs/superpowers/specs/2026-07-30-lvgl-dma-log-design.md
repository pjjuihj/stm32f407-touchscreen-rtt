# LVGL DMA 日志系统设计

**日期**: 2026-07-30  
**状态**: 已批准  
**目标**: 解决 LVGL 9.5 日志启用后 MCU 卡死问题

---

## 1. 问题背景

### 1.1 当前状态

- LVGL 9.5 日志回调 `lv_log_print_g_cb` 直接调用 `USART1_SendString`（阻塞式）
- `gui_log_flush()` 是空函数
- 所有 UART 输出都是阻塞的（`HAL_UART_Transmit` + `HAL_MAX_DELAY`）

### 1.2 卡死原因

1. LVGL 内部调用日志时，阻塞式 UART 发送导致系统卡死
2. UART 发送期间，SysTick 中断可能被延迟
3. `delay_us` 依赖 SysTick，导致进一步卡死

### 1.3 目标

- 启用 LVGL 日志（WARN 级别），不卡死系统
- 使用 DMA 非阻塞输出，CPU 零等待
- 合并 LVGL 日志和自定义 `Log_Write` 系统

---

## 2. 架构设计

### 2.1 系统架构

```
┌─────────────────┐          ┌─────────────────┐
│   LVGL 日志     │          │  自定义 Log_Write │
│ (lv_log_print_g_cb) │          │  (Log_Write)     │
└────────┬────────┘          └────────┬────────┘
         │                            │
         ▼                            ▼
┌─────────────────┐          ┌─────────────────┐
│ LVGL 环形缓冲区  │          │ 自定义环形缓冲区  │
│   (1024 bytes)  │          │   (1024 bytes)  │
│ head/tail/count │          │ head/tail/count │
└────────┬────────┘          └────────┬────────┘
         │                            │
         └─────────────┬──────────────┘
                       │
                       ▼
┌─────────────────────────────────────────┐
│         DMA 调度器 (优先级仲裁)           │
│   - LVGL 日志优先（调试更关键）            │
│   - 检查 LVGL 缓冲区 → 有数据则发送       │
│   - 否则检查自定义缓冲区 → 有数据则发送    │
│   - DMA 空闲时才启动新传输                │
└────────────────────┬────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────┐
│           UART1 (115200 baud)            │
│        HAL_UART_Transmit_DMA            │
└─────────────────────────────────────────┘
```

### 2.2 数据流

1. LVGL 日志写入 LVGL 环形缓冲区
2. 自定义日志写入自定义环形缓冲区
3. `gui_log_flush()` 在主循环中执行调度：
   - 优先检查 LVGL 缓冲区（调试关键）
   - LVGL 无数据时检查自定义缓冲区
   - DMA 空闲且有数据时启动传输
4. DMA 完成回调触发下一轮调度

### 2.3 设计约束

- 两个缓冲区独立管理，互不干扰
- DMA 调度器确保同一时刻只有一个 DMA 传输
- LVGL 日志回调中不能调用任何 LVGL API
- 优先级：LVGL 日志 > 自定义日志（调试时更需要 LVGL 输出）

---

## 3. 数据结构

### 3.1 环形缓冲区结构

```c
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

### 3.2 DMA 调度器状态

```c
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
    uint8_t tx_buf[256];        // DMA 发送缓冲区（从环形缓冲区拷贝）
    uint16_t tx_len;            // 当前发送长度
} dma_scheduler_t;
```

### 3.3 全局实例

```c
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

### 3.4 接口定义

```c
// gui_driver.h

/**
 * @brief 初始化日志系统（必须在 lv_init 之前调用）
 */
void gui_log_init(void);

/**
 * @brief 主循环调用 - 调度 DMA 发送
 */
void gui_log_flush(void);

/**
 * @brief 写入自定义日志（供 Log_Write 调用）
 * @param str: 要发送的字符串
 */
void gui_log_write(const char *str);
```

---

## 4. 核心函数实现

### 4.1 环形缓冲区操作

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

### 4.2 LVGL 日志回调

```c
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

### 4.3 DMA 调度器

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
 * @brief 启动 DMA 发送
 */
static void start_dma_tx(void)
{
    if(dma_scheduler.state != DMA_IDLE) return;
    if(dma_scheduler.tx_len == 0) return;
    
    dma_scheduler.state = DMA_TX_LVGL;  // 或 DMA_TX_CUSTOM
    HAL_UART_Transmit_DMA(&huart1, dma_scheduler.tx_buf, dma_scheduler.tx_len);
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

### 4.4 DMA 完成回调

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
```

### 4.5 公共接口

```c
/**
 * @brief 初始化日志系统
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

/**
 * @brief 主循环调用 - 调度 DMA 发送
 */
void gui_log_flush(void)
{
    dma_scheduler_run();
}

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

---

## 5. 与现有代码集成

### 5.1 修改 `gui_driver.c`

**删除的内容**：
- 旧的 `lv_log_print_g_cb` 函数（直接调用 USART 的版本）
- 旧的 `gui_log_flush` 空函数

**新增的内容**：
- 环形缓冲区结构和操作函数
- 新的 `lv_log_print_g_cb`（写入缓冲区）
- DMA 调度器
- `gui_log_init`、`gui_log_flush`、`gui_log_write` 实现

**保留的内容**：
- 显示驱动（`disp_flush_cb`、`gui_disp_init`）
- 触摸驱动（`touch_read_cb`、`gui_touch_init`）
- 时钟驱动（`gui_tick_get`、`gui_tick_init`）

### 5.2 修改 `gui_driver.h`

**新增声明**：
```c
/**
 * @brief 写入自定义日志（供 Log_Write 调用）
 */
void gui_log_write(const char *str);
```

### 5.3 修改 `usart.c`

**新增 DMA 支持**：
```c
// 启用 USART1 DMA 时钟
__HAL_RCC_DMA2_CLK_ENABLE();

// 配置 DMA2 Stream7 (USART1_TX)
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

// 链接 DMA 到 UART
__HAL_LINKDMA(&huart1, hdmatx, hdma_usart1_tx);

// 配置 DMA 中断
HAL_NVIC_SetPriority(DMA2_Stream7_IRQn, 0, 0);
HAL_NVIC_EnableIRQ(DMA2_Stream7_IRQn);
```

**新增中断处理**：
```c
// DMA2 Stream7 中断处理
void DMA2_Stream7_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart1_tx);
}
```

### 5.4 修改 `log.c`

**修改 `Log_Write` 函数**：
```c
void Log_Write(Module module, Level level, const char *message)
{
    // ... 格式化日志字符串到 log_str ...
    
    // 旧代码: USART1_SendString(log_str);
    // 新代码: 写入自定义日志缓冲区
    gui_log_write(log_str);
}
```

### 5.5 修改 `main.c`

**初始化顺序调整**：
```c
// 1. USART1 初始化（包含 DMA 配置）
USART1_Init();

// 2. LVGL 日志初始化（注册回调）
gui_log_init();

// 3. LVGL 初始化
lv_init();

// ... 其他初始化 ...

// 主循环
while(1) {
    // ... 其他处理 ...
    
    gui_log_flush();  // 调度 DMA 发送
    lv_task_handler();
    delay_ms(5);
}
```

### 5.6 修改 `lv_conf.h`（根目录）

**日志配置**：
```c
#define LV_USE_LOG             1
#if LV_USE_LOG
    #define LV_LOG_LEVEL        LV_LOG_LEVEL_WARN  // WARN 级别
    #define LV_LOG_PRINTF       0     // 使用自定义回调
#endif
```

---

## 6. 错误处理与边界情况

### 6.1 缓冲区溢出处理

```c
/**
 * @brief 写入单个字符到环形缓冲区
 * @note  缓冲区满时静默丢弃字符，不阻塞
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
    // 缓冲区满时丢弃字符（静默）
    
    __set_PRIMASK(primask);
}
```

**设计决策**：
- 溢出时丢弃字符，不阻塞系统
- 不输出错误信息（避免递归）
- 可选：添加溢出计数器用于调试

### 6.2 DMA 错误处理

```c
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

### 6.3 递归保护

```c
/**
 * @brief LVGL 日志回调 - 带递归保护
 */
static void lv_log_print_g_cb(lv_log_level_t level, const char *buf)
{
    // 递归保护：防止日志回调中触发其他日志
    static volatile bool in_log_cb = false;
    if(in_log_cb) return;
    in_log_cb = true;
    
    // ... 写入缓冲区 ...
    
    in_log_cb = false;
}
```

### 6.4 DMA 状态机保护

```c
/**
 * @brief 启动 DMA 发送（带状态检查）
 */
static void start_dma_tx(void)
{
    // 状态检查：只在 IDLE 状态时启动
    if(dma_scheduler.state != DMA_IDLE) return;
    
    // 长度检查：只在有数据时启动
    if(dma_scheduler.tx_len == 0) return;
    
    // 启动 DMA
    dma_scheduler.state = DMA_TX_LVGL;
    HAL_UART_Transmit_DMA(&huart1, dma_scheduler.tx_buf, dma_scheduler.tx_len);
}
```

### 6.5 中断安全

```c
/**
 * @brief 写入环形缓冲区（中断安全）
 * @note  使用 PRIMASK 保护临界区
 */
static void ring_buf_put(ring_buf_t *ring, char c)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    
    // ... 操作缓冲区 ...
    
    __set_PRIMASK(primask);
}

/**
 * @brief 读取环形缓冲区（主循环调用，无需保护）
 */
static int ring_buf_get(ring_buf_t *ring)
{
    // 主循环中调用，无需关中断
    if(ring->count == 0) return -1;
    
    char c = ring->buf[ring->tail];
    ring->tail = (ring->tail + 1) % ring->size;
    ring->count--;
    return c;
}
```

---

## 7. 测试策略

### 7.1 单元测试

| 测试用例 | 验证内容 |
|----------|----------|
| `test_ring_buf_put_get` | 环形缓冲区基本读写 |
| `test_ring_buf_overflow` | 缓冲区满时丢弃字符 |
| `test_ring_buf_interrupt安全` | 中断安全保护 |
| `test_dma_scheduler_priority` | LVGL 优先于自定义日志 |
| `test_dma_state_machine` | 状态机转换正确 |
| `test_lv_log_callback` | LVGL 回调写入正确缓冲区 |
| `test_recursive_protection` | 递归保护生效 |

### 7.2 集成测试

| 测试场景 | 预期结果 |
|----------|----------|
| 启用 LVGL 日志 + 无自定义日志 | LVGL 日志正常输出，无卡死 |
| 禁用 LVGL 日志 + 有自定义日志 | 自定义日志正常输出 |
| 两者同时输出 | 交替输出，LVGL 优先 |
| 高频日志（TRACE 级别） | 不丢帧，不卡死 |
| 缓冲区满 | 丢弃新字符，不阻塞 |

### 7.3 硬件验证

1. **基础功能**：烧录后观察串口输出，确认日志正常
2. **性能测试**：测量 LVGL 刷新率，确认无明显下降
3. **压力测试**：启用 TRACE 级别，运行 10 分钟，确认无卡死
4. **边界测试**：满缓冲区状态下运行，确认无崩溃

---

## 8. 参考资源

- [LVGL 9.5 官方文档 - Logging](https://docs.lvgl.io/9.5/debugging/log.html)
- [LVGL 9.5 API - lv_log.h](https://docs.lvgl.io/9.5/API/misc/lv_log_h.html)
- [STM32F407 参考手册 - DMA](https://www.st.com/resource/en/reference_manual/rm0090-stm32f405415-stm32f407417-stm32f427437-and-stm32f429439-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)

---

## 9. 变更历史

| 日期 | 版本 | 变更内容 |
|------|------|----------|
| 2026-07-30 | 1.0 | 初始设计 |
