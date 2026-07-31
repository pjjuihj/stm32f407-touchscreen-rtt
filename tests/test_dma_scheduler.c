/**
 * @file test_dma_scheduler.c
 * @brief DMA 调度器和公共 API 单元测试
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

/* 模拟 STM32 HAL 类型 */
typedef struct {
    int dummy;
} UART_HandleTypeDef;

typedef struct {
    int dummy;
} DMA_HandleTypeDef;

#define USART1 ((void*)0)
#define HAL_OK 0

/* 模拟全局变量 */
UART_HandleTypeDef huart1;
DMA_HandleTypeDef hdma_usart1_tx;

/* 模拟 DMA 状态 */
static int mock_dma_started = 0;
static char mock_dma_buf[256];
static int mock_dma_len = 0;

/* 模拟 HAL 函数 */
void HAL_UART_Transmit_DMA(UART_HandleTypeDef *huart, uint8_t *data, uint16_t len)
{
    mock_dma_started = 1;
    memcpy(mock_dma_buf, data, len);
    mock_dma_len = len;
}

/* 环形缓冲区结构 */
typedef struct {
    volatile unsigned short head;
    volatile unsigned short tail;
    volatile unsigned short count;
    unsigned short size;
    char *buf;
} ring_buf_t;

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
        HAL_UART_Transmit_DMA(&huart1, dma_scheduler.tx_buf, dma_scheduler.tx_len);
        return;
    }

    if(ring_buf_count(&custom_ring) > 0) {
        dma_scheduler.tx_len = fill_dma_buf(&custom_ring, sizeof(dma_scheduler.tx_buf));
        dma_scheduler.state = DMA_TX_CUSTOM;
        HAL_UART_Transmit_DMA(&huart1, dma_scheduler.tx_buf, dma_scheduler.tx_len);
        return;
    }
}

/* 公共 API 函数 */
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

static void gui_log_flush(void)
{
    dma_scheduler_run();
}

static void gui_log_write(const char *str)
{
    if(!str) return;
    for(const char *p = str; *p; p++) {
        ring_buf_put(&custom_ring, *p);
    }
}

/* 测试函数 */
void test_gui_log_init(void)
{
    printf("测试 gui_log_init...\n");

    gui_log_init();

    assert(ring_buf_count(&lvgl_ring) == 0);
    assert(ring_buf_count(&custom_ring) == 0);
    assert(dma_scheduler.state == DMA_IDLE);

    printf("  ✓ 测试通过\n");
}

void test_gui_log_write_basic(void)
{
    printf("测试 gui_log_write 基本功能...\n");

    gui_log_init();
    gui_log_write("Hello");

    assert(ring_buf_count(&custom_ring) == 5);

    char c;
    c = ring_buf_get(&custom_ring);
    assert(c == 'H');
    c = ring_buf_get(&custom_ring);
    assert(c == 'e');
    c = ring_buf_get(&custom_ring);
    assert(c == 'l');
    c = ring_buf_get(&custom_ring);
    assert(c == 'l');
    c = ring_buf_get(&custom_ring);
    assert(c == 'o');

    printf("  ✓ 测试通过\n");
}

void test_gui_log_write_null(void)
{
    printf("测试 gui_log_write 空指针...\n");

    gui_log_init();
    gui_log_write(NULL);

    assert(ring_buf_count(&custom_ring) == 0);

    printf("  ✓ 测试通过\n");
}

void test_gui_log_write_empty(void)
{
    printf("测试 gui_log_write 空字符串...\n");

    gui_log_init();
    gui_log_write("");

    assert(ring_buf_count(&custom_ring) == 0);

    printf("  ✓ 测试通过\n");
}

void test_dma_scheduler_priority(void)
{
    printf("测试 DMA 调度器优先级...\n");

    gui_log_init();

    /* 写入两个缓冲区 */
    ring_buf_put(&lvgl_ring, 'L');
    ring_buf_put(&custom_ring, 'C');

    /* 调度器应该优先处理 LVGL 缓冲区 */
    mock_dma_started = 0;
    dma_scheduler_run();

    assert(mock_dma_started == 1);
    assert(dma_scheduler.state == DMA_TX_LVGL);
    assert(mock_dma_buf[0] == 'L');

    printf("  ✓ 测试通过\n");
}

void test_dma_scheduler_custom_fallback(void)
{
    printf("测试 DMA 调度器自定义日志回退...\n");

    gui_log_init();

    /* 只写入自定义缓冲区 */
    ring_buf_put(&custom_ring, 'C');

    /* 调度器应该处理自定义缓冲区 */
    mock_dma_started = 0;
    dma_scheduler_run();

    assert(mock_dma_started == 1);
    assert(dma_scheduler.state == DMA_TX_CUSTOM);
    assert(mock_dma_buf[0] == 'C');

    printf("  ✓ 测试通过\n");
}

void test_dma_scheduler_idle(void)
{
    printf("测试 DMA 调度器空闲状态...\n");

    gui_log_init();

    /* 空缓冲区，不应该启动 DMA */
    mock_dma_started = 0;
    dma_scheduler_run();

    assert(mock_dma_started == 0);
    assert(dma_scheduler.state == DMA_IDLE);

    printf("  ✓ 测试通过\n");
}

void test_dma_scheduler_busy(void)
{
    printf("测试 DMA 调度器忙碌状态...\n");

    gui_log_init();

    /* 模拟 DMA 正在发送 */
    dma_scheduler.state = DMA_TX_LVGL;

    /* 写入新数据 */
    ring_buf_put(&lvgl_ring, 'L');

    /* 调度器不应该启动新的 DMA */
    mock_dma_started = 0;
    dma_scheduler_run();

    assert(mock_dma_started == 0);
    assert(dma_scheduler.state == DMA_TX_LVGL);

    printf("  ✓ 测试通过\n");
}

void test_gui_log_flush(void)
{
    printf("测试 gui_log_flush...\n");

    gui_log_init();

    /* 写入数据 */
    gui_log_write("Test");

    /* 刷新缓冲区 */
    mock_dma_started = 0;
    gui_log_flush();

    assert(mock_dma_started == 1);
    assert(dma_scheduler.state == DMA_TX_CUSTOM);
    assert(mock_dma_len == 4);

    printf("  ✓ 测试通过\n");
}

void test_dma_scheduler_multiple_flushes(void)
{
    printf("测试 DMA 调度器多次刷新...\n");

    gui_log_init();

    /* 写入数据 */
    gui_log_write("Message 1");

    /* 第一次刷新 */
    mock_dma_started = 0;
    gui_log_flush();

    assert(mock_dma_started == 1);
    assert(dma_scheduler.state == DMA_TX_CUSTOM);

    /* 模拟 DMA 完成 */
    dma_scheduler.state = DMA_IDLE;

    /* 写入更多数据 */
    gui_log_write("Message 2");

    /* 第二次刷新 */
    mock_dma_started = 0;
    gui_log_flush();

    assert(mock_dma_started == 1);
    assert(dma_scheduler.state == DMA_TX_CUSTOM);

    printf("  ✓ 测试通过\n");
}

int main(void)
{
    printf("=== DMA 调度器和公共 API 单元测试 ===\n\n");

    test_gui_log_init();
    test_gui_log_write_basic();
    test_gui_log_write_null();
    test_gui_log_write_empty();
    test_dma_scheduler_priority();
    test_dma_scheduler_custom_fallback();
    test_dma_scheduler_idle();
    test_dma_scheduler_busy();
    test_gui_log_flush();
    test_dma_scheduler_multiple_flushes();

    printf("\n=== 所有测试通过 ===\n");
    return 0;
}
