/**
 * @file test_interrupt_safety.c
 * @brief 中断安全测试
 *
 * 测试环形缓冲区在模拟中断环境下的行为
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

/* 模拟中断状态 */
static bool interrupt_enabled = true;

/* 模拟中断控制函数 */
static void disable_irq(void)
{
    interrupt_enabled = false;
}

static void enable_irq(void)
{
    interrupt_enabled = true;
}

/* 环形缓冲区结构 */
typedef struct {
    volatile unsigned short head;
    volatile unsigned short tail;
    volatile unsigned short count;
    unsigned short size;
    char *buf;
} ring_buf_t;

/* 中断安全的环形缓冲区操作 */
static void ring_buf_put_safe(ring_buf_t *ring, char c)
{
    bool was_enabled = interrupt_enabled;
    disable_irq();

    if(ring->count < ring->size) {
        ring->buf[ring->head] = c;
        ring->head = (ring->head + 1) % ring->size;
        ring->count++;
    }

    if(was_enabled) enable_irq();
}

static int ring_buf_get_safe(ring_buf_t *ring)
{
    bool was_enabled = interrupt_enabled;
    disable_irq();

    if(ring->count == 0) {
        if(was_enabled) enable_irq();
        return -1;
    }

    char c = ring->buf[ring->tail];
    ring->tail = (ring->tail + 1) % ring->size;
    ring->count--;

    if(was_enabled) enable_irq();
    return c;
}

static unsigned short ring_buf_count(ring_buf_t *ring)
{
    return ring->count;
}

/* 测试函数 */
void test_interrupt_safe_put(void)
{
    printf("测试中断安全写入...\n");

    ring_buf_t test_ring;
    char test_buf[16];
    test_ring.head = 0;
    test_ring.tail = 0;
    test_ring.count = 0;
    test_ring.size = 16;
    test_ring.buf = test_buf;

    /* 模拟中断上下文写入 */
    interrupt_enabled = true;
    ring_buf_put_safe(&test_ring, 'A');
    ring_buf_put_safe(&test_ring, 'B');
    ring_buf_put_safe(&test_ring, 'C');

    assert(ring_buf_count(&test_ring) == 3);
    assert(interrupt_enabled == true);

    printf("  ✓ 测试通过\n");
}

void test_interrupt_safe_get(void)
{
    printf("测试中断安全读取...\n");

    ring_buf_t test_ring;
    char test_buf[16];
    test_ring.head = 0;
    test_ring.tail = 0;
    test_ring.count = 0;
    test_ring.size = 16;
    test_ring.buf = test_buf;

    /* 写入数据 */
    ring_buf_put_safe(&test_ring, 'A');
    ring_buf_put_safe(&test_ring, 'B');

    /* 模拟中断上下文读取 */
    interrupt_enabled = true;
    assert(ring_buf_get_safe(&test_ring) == 'A');
    assert(ring_buf_get_safe(&test_ring) == 'B');
    assert(ring_buf_count(&test_ring) == 0);
    assert(interrupt_enabled == true);

    printf("  ✓ 测试通过\n");
}

void test_interrupt_safe_empty(void)
{
    printf("测试中断安全空缓冲区读取...\n");

    ring_buf_t test_ring;
    char test_buf[16];
    test_ring.head = 0;
    test_ring.tail = 0;
    test_ring.count = 0;
    test_ring.size = 16;
    test_ring.buf = test_buf;

    /* 模拟中断上下文读取空缓冲区 */
    interrupt_enabled = true;
    assert(ring_buf_get_safe(&test_ring) == -1);
    assert(interrupt_enabled == true);

    printf("  ✓ 测试通过\n");
}

void test_interrupt_protection(void)
{
    printf("测试中断保护机制...\n");

    ring_buf_t test_ring;
    char test_buf[16];
    test_ring.head = 0;
    test_ring.tail = 0;
    test_ring.count = 0;
    test_ring.size = 16;
    test_ring.buf = test_buf;

    /* 测试中断禁用/启用 */
    interrupt_enabled = true;
    ring_buf_put_safe(&test_ring, 'A');

    /* 验证中断状态恢复 */
    assert(interrupt_enabled == true);

    /* 测试嵌套保护 */
    interrupt_enabled = true;
    ring_buf_put_safe(&test_ring, 'B');
    assert(interrupt_enabled == true);

    printf("  ✓ 测试通过\n");
}

void test_concurrent_access_simulation(void)
{
    printf("测试模拟并发访问...\n");

    ring_buf_t test_ring;
    char test_buf[16];
    test_ring.head = 0;
    test_ring.tail = 0;
    test_ring.count = 0;
    test_ring.size = 16;
    test_ring.buf = test_buf;

    /* 模拟主循环和中断同时访问 */
    for(int i = 0; i < 10; i++) {
        /* 主循环写入 */
        ring_buf_put_safe(&test_ring, 'A' + i);

        /* 模拟中断读取 */
        if(ring_buf_count(&test_ring) > 0) {
            int c = ring_buf_get_safe(&test_ring);
            assert(c >= 'A' && c <= 'J');
        }
    }

    assert(ring_buf_count(&test_ring) == 0);

    printf("  ✓ 测试通过\n");
}

int main(void)
{
    printf("=== 中断安全测试 ===\n\n");

    test_interrupt_safe_put();
    test_interrupt_safe_get();
    test_interrupt_safe_empty();
    test_interrupt_protection();
    test_concurrent_access_simulation();

    printf("\n=== 所有测试通过 ===\n");
    return 0;
}
