/**
 * @file test_dma_log.c
 * @brief DMA 日志系统单元测试
 *
 * 测试环形缓冲区、DMA 调度器和公共 API
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>

/* 模拟 STM32 HAL 类型 */
typedef struct {
    int dummy;
} UART_HandleTypeDef;

typedef struct {
    int dummy;
} DMA_HandleTypeDef;

#define USART1 ((void*)0)
#define HAL_OK 0
#define HAL_MAX_DELAY 0xFFFFFFFF

/* 模拟全局变量 */
UART_HandleTypeDef huart1;
DMA_HandleTypeDef hdma_usart1_tx;

/* 模拟函数 */
static int mock_dma_started = 0;
static char mock_dma_buf[256];
static int mock_dma_len = 0;

void HAL_UART_Transmit_DMA(UART_HandleTypeDef *huart, uint8_t *data, uint16_t len)
{
    mock_dma_started = 1;
    memcpy(mock_dma_buf, data, len);
    mock_dma_len = len;
}

/* 包含被测试的代码 */
#include "../GUI/gui_driver.c"

/* 测试函数 */
void test_ring_buf_put_get(void)
{
    printf("测试 ring_buf_put 和 ring_buf_get...\n");

    /* 初始化环形缓冲区 */
    ring_buf_t test_ring;
    char test_buf[16];
    test_ring.head = 0;
    test_ring.tail = 0;
    test_ring.count = 0;
    test_ring.size = 16;
    test_ring.buf = test_buf;

    /* 测试写入和读取 */
    ring_buf_put(&test_ring, 'A');
    ring_buf_put(&test_ring, 'B');
    ring_buf_put(&test_ring, 'C');

    assert(ring_buf_count(&test_ring) == 3);
    assert(ring_buf_get(&test_ring) == 'A');
    assert(ring_buf_get(&test_ring) == 'B');
    assert(ring_buf_get(&test_ring) == 'C');
    assert(ring_buf_count(&test_ring) == 0);

    printf("  ✓ 测试通过\n");
}

void test_ring_buf_overflow(void)
{
    printf("测试环形缓冲区溢出...\n");

    ring_buf_t test_ring;
    char test_buf[4];  /* 只能存 3 个字符 */
    test_ring.head = 0;
    test_ring.tail = 0;
    test_ring.count = 0;
    test_ring.size = 4;
    test_ring.buf = test_buf;

    /* 写入 4 个字符，第 4 个应该被丢弃 */
    ring_buf_put(&test_ring, 'A');
    ring_buf_put(&test_ring, 'B');
    ring_buf_put(&test_ring, 'C');
    ring_buf_put(&test_ring, 'D');  /* 应该被丢弃 */

    assert(ring_buf_count(&test_ring) == 3);
    assert(ring_buf_get(&test_ring) == 'A');
    assert(ring_buf_get(&test_ring) == 'B');
    assert(ring_buf_get(&test_ring) == 'C');
    assert(ring_buf_count(&test_ring) == 0);

    printf("  ✓ 测试通过\n");
}

void test_ring_buf_empty(void)
{
    printf("测试空环形缓冲区读取...\n");

    ring_buf_t test_ring;
    char test_buf[4];
    test_ring.head = 0;
    test_ring.tail = 0;
    test_ring.count = 0;
    test_ring.size = 4;
    test_ring.buf = test_buf;

    assert(ring_buf_get(&test_ring) == -1);
    assert(ring_buf_count(&test_ring) == 0);

    printf("  ✓ 测试通过\n");
}

void test_dma_scheduler_priority(void)
{
    printf("测试 DMA 调度器优先级...\n");

    /* 初始化 */
    memset(&lvgl_ring, 0, sizeof(lvgl_ring));
    memset(&custom_ring, 0, sizeof(custom_ring));
    memset(&dma_scheduler, 0, sizeof(dma_scheduler));

    char lvgl_buf[64];
    char custom_buf[64];
    lvgl_ring.buf = lvgl_buf;
    lvgl_ring.size = 64;
    custom_ring.buf = custom_buf;
    custom_ring.size = 64;

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

void test_dma_scheduler_idle(void)
{
    printf("测试 DMA 调度器空闲状态...\n");

    /* 初始化 */
    memset(&lvgl_ring, 0, sizeof(lvgl_ring));
    memset(&custom_ring, 0, sizeof(custom_ring));
    memset(&dma_scheduler, 0, sizeof(dma_scheduler));

    char lvgl_buf[64];
    char custom_buf[64];
    lvgl_ring.buf = lvgl_buf;
    lvgl_ring.size = 64;
    custom_ring.buf = custom_buf;
    custom_ring.size = 64;

    /* 空缓冲区，不应该启动 DMA */
    mock_dma_started = 0;
    dma_scheduler_run();

    assert(mock_dma_started == 0);
    assert(dma_scheduler.state == DMA_IDLE);

    printf("  ✓ 测试通过\n");
}

void test_gui_log_write(void)
{
    printf("测试 gui_log_write...\n");

    /* 初始化 */
    memset(&custom_ring, 0, sizeof(custom_ring));
    char custom_buf[64];
    custom_ring.buf = custom_buf;
    custom_ring.size = 64;

    /* 写入字符串 */
    gui_log_write("Hello");

    assert(ring_buf_count(&custom_ring) == 5);
    assert(ring_buf_get(&custom_ring) == 'H');
    assert(ring_buf_get(&custom_ring) == 'e');
    assert(ring_buf_get(&custom_ring) == 'l');
    assert(ring_buf_get(&custom_ring) == 'l');
    assert(ring_buf_get(&custom_ring) == 'o');

    printf("  ✓ 测试通过\n");
}

void test_gui_log_write_null(void)
{
    printf("测试 gui_log_write 空指针...\n");

    /* 初始化 */
    memset(&custom_ring, 0, sizeof(custom_ring));
    char custom_buf[64];
    custom_ring.buf = custom_buf;
    custom_ring.size = 64;

    /* 写入空指针，不应该崩溃 */
    gui_log_write(NULL);

    assert(ring_buf_count(&custom_ring) == 0);

    printf("  ✓ 测试通过\n");
}

int main(void)
{
    printf("=== DMA 日志系统单元测试 ===\n\n");

    test_ring_buf_put_get();
    test_ring_buf_overflow();
    test_ring_buf_empty();
    test_dma_scheduler_priority();
    test_dma_scheduler_idle();
    test_gui_log_write();
    test_gui_log_write_null();

    printf("\n=== 所有测试通过 ===\n");
    return 0;
}
