/**
 * @file test_dma_callback.c
 * @brief DMA 完成回调测试
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

/* 模拟 STM32 HAL 类型 */
typedef struct {
    int Instance;
} UART_HandleTypeDef;

typedef struct {
    int dummy;
} DMA_HandleTypeDef;

#define USART1 ((void*)0x40011000)  /* 模拟 USART1 地址 */

/* 模拟 DMA 状态 */
static int mock_dma_started = 0;
static int mock_dma_completed = 0;
static char mock_dma_buf[256];
static int mock_dma_len = 0;

/* 环形缓冲区结构 */
typedef struct {
    volatile unsigned short head;
    volatile unsigned short tail;
    volatile unsigned short count;
    unsigned short size;
    char *buf;
} ring_buf_t;

/* DMA 调度器状态 */
typedef enum {
    DMA_IDLE = 0,
    DMA_TX_LVGL,
    DMA_TX_CUSTOM,
} dma_state_t;

typedef struct {
    dma_state_t state;
    uint8_t tx_buf[256];
    uint16_t tx_len;
} dma_scheduler_t;

/* 全局变量 */
static ring_buf_t lvgl_ring;
static ring_buf_t custom_ring;
static dma_scheduler_t dma_scheduler;
static char lvgl_buf[1024];
static char custom_buf[1024];

/* 环形缓冲区操作函数 */
static void ring_buf_put(ring_buf_t *ring, char c)
{
    if(ring->count < ring->size) {
        ring->buf[ring->head] = c;
        ring->head = (ring->head + 1) % ring->size;
        ring->count++;
    }
}

static int ring_buf_get(ring_buf_t *ring)
{
    if(ring->count == 0) return -1;

    char c = ring->buf[ring->tail];
    ring->tail = (ring->tail + 1) % ring->size;
    ring->count--;
    return c;
}

static unsigned short ring_buf_count(ring_buf_t *ring)
{
    return ring->count;
}

/* 前向声明 */
static void dma_scheduler_run(void);

/* 模拟 HAL 函数 */
static void HAL_UART_Transmit_DMA(UART_HandleTypeDef *huart, uint8_t *data, uint16_t len)
{
    mock_dma_started = 1;
    memcpy(mock_dma_buf, data, len);
    mock_dma_len = len;
}

/* DMA 调度器函数 */
static uint16_t fill_dma_buf(ring_buf_t *ring, uint16_t max_len)
{
    uint16_t len = 0;
    int c;

    while(len < max_len && (c = ring_buf_get(ring)) != -1) {
        dma_scheduler.tx_buf[len++] = (uint8_t)c;
    }

    return len;
}

static void dma_scheduler_run(void)
{
    if(dma_scheduler.state != DMA_IDLE) return;

    if(ring_buf_count(&lvgl_ring) > 0) {
        dma_scheduler.tx_len = fill_dma_buf(&lvgl_ring, sizeof(dma_scheduler.tx_buf));
        dma_scheduler.state = DMA_TX_LVGL;
        HAL_UART_Transmit_DMA((UART_HandleTypeDef*)USART1, dma_scheduler.tx_buf, dma_scheduler.tx_len);
        return;
    }

    if(ring_buf_count(&custom_ring) > 0) {
        dma_scheduler.tx_len = fill_dma_buf(&custom_ring, sizeof(dma_scheduler.tx_buf));
        dma_scheduler.state = DMA_TX_CUSTOM;
        HAL_UART_Transmit_DMA((UART_HandleTypeDef*)USART1, dma_scheduler.tx_buf, dma_scheduler.tx_len);
        return;
    }
}

/* DMA 完成回调 */
static void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance != (int)USART1) return;

    dma_scheduler.state = DMA_IDLE;
    dma_scheduler.tx_len = 0;

    /* 立即检查是否有更多数据要发送 */
    dma_scheduler_run();
}

/* 初始化函数 */
static void gui_log_init(void)
{
    lvgl_ring.head = 0;
    lvgl_ring.tail = 0;
    lvgl_ring.count = 0;
    lvgl_ring.size = 1024;
    lvgl_ring.buf = lvgl_buf;

    custom_ring.head = 0;
    custom_ring.tail = 0;
    custom_ring.count = 0;
    custom_ring.size = 1024;
    custom_ring.buf = custom_buf;

    memset(&dma_scheduler, 0, sizeof(dma_scheduler));
}

/* 测试函数 */
void test_dma_callback_resets_state(void)
{
    printf("测试 DMA 回调重置状态...\n");

    gui_log_init();

    /* 模拟 DMA 正在发送 */
    dma_scheduler.state = DMA_TX_LVGL;
    dma_scheduler.tx_len = 100;

    /* 调用回调 */
    UART_HandleTypeDef huart;
    huart.Instance = (int)USART1;
    HAL_UART_TxCpltCallback(&huart);

    assert(dma_scheduler.state == DMA_IDLE);
    assert(dma_scheduler.tx_len == 0);

    printf("  ✓ 测试通过\n");
}

void test_dma_callback_triggers_next_transfer(void)
{
    printf("测试 DMA 回调触发下一次传输...\n");

    gui_log_init();

    /* 写入数据 */
    ring_buf_put(&custom_ring, 'A');
    ring_buf_put(&custom_ring, 'B');

    /* 模拟第一次 DMA 传输 */
    dma_scheduler.state = DMA_TX_CUSTOM;
    mock_dma_started = 0;

    /* 调用回调 */
    UART_HandleTypeDef huart;
    huart.Instance = (int)USART1;
    HAL_UART_TxCpltCallback(&huart);

    /* 应该触发新的 DMA 传输 */
    assert(mock_dma_started == 1);
    assert(dma_scheduler.state == DMA_TX_CUSTOM);
    assert(mock_dma_buf[0] == 'A');
    assert(mock_dma_buf[1] == 'B');

    printf("  ✓ 测试通过\n");
}

void test_dma_callback_no_data(void)
{
    printf("测试 DMA 回调无数据...\n");

    gui_log_init();
    mock_dma_started = 0;  /* 重置标志 */

    /* 模拟 DMA 正在发送 */
    dma_scheduler.state = DMA_TX_LVGL;

    /* 调用回调（缓冲区为空） */
    UART_HandleTypeDef huart;
    huart.Instance = (int)USART1;
    HAL_UART_TxCpltCallback(&huart);

    /* 应该返回空闲状态 */
    assert(dma_scheduler.state == DMA_IDLE);

    printf("  ✓ 测试通过\n");
}

void test_dma_callback_wrong_instance(void)
{
    printf("测试 DMA 回调错误实例...\n");

    gui_log_init();

    /* 模拟 DMA 正在发送 */
    dma_scheduler.state = DMA_TX_LVGL;

    /* 调用回调（错误的 UART 实例） */
    UART_HandleTypeDef huart;
    huart.Instance = 0x12345678;  /* 错误的地址 */
    HAL_UART_TxCpltCallback(&huart);

    /* 状态不应该改变 */
    assert(dma_scheduler.state == DMA_TX_LVGL);

    printf("  ✓ 测试通过\n");
}

void test_dma_callback_sequential_transfers(void)
{
    printf("测试 DMA 回调顺序传输...\n");

    gui_log_init();

    /* 写入数据 */
    ring_buf_put(&custom_ring, '1');
    ring_buf_put(&custom_ring, '2');
    ring_buf_put(&custom_ring, '3');

    /* 模拟第一次 DMA 传输 */
    dma_scheduler.state = DMA_TX_CUSTOM;
    mock_dma_started = 0;

    /* 调用回调 */
    UART_HandleTypeDef huart;
    huart.Instance = (int)USART1;
    HAL_UART_TxCpltCallback(&huart);

    /* 第一次传输完成，应该触发第二次 */
    assert(mock_dma_started == 1);
    assert(dma_scheduler.state == DMA_TX_CUSTOM);

    /* 模拟第二次 DMA 传输完成 */
    dma_scheduler.state = DMA_TX_CUSTOM;
    HAL_UART_TxCpltCallback(&huart);

    /* 应该触发第三次传输 */
    assert(mock_dma_started == 1);

    printf("  ✓ 测试通过\n");
}

int main(void)
{
    printf("=== DMA 完成回调测试 ===\n\n");

    test_dma_callback_resets_state();
    test_dma_callback_triggers_next_transfer();
    test_dma_callback_no_data();
    test_dma_callback_wrong_instance();
    test_dma_callback_sequential_transfers();

    printf("\n=== 所有测试通过 ===\n");
    return 0;
}
