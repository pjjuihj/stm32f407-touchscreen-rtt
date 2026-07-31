/**
 * @file test_ring_buffer.c
 * @brief 环形缓冲区单元测试
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>

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

/* 测试函数 */
void test_ring_buf_put_get(void)
{
    printf("测试 ring_buf_put 和 ring_buf_get...\n");

    ring_buf_t test_ring;
    char test_buf[16];
    test_ring.head = 0;
    test_ring.tail = 0;
    test_ring.count = 0;
    test_ring.size = 16;
    test_ring.buf = test_buf;

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
    char test_buf[4];
    test_ring.head = 0;
    test_ring.tail = 0;
    test_ring.count = 0;
    test_ring.size = 4;
    test_ring.buf = test_buf;

    ring_buf_put(&test_ring, 'A');
    ring_buf_put(&test_ring, 'B');
    ring_buf_put(&test_ring, 'C');
    ring_buf_put(&test_ring, 'D');
    ring_buf_put(&test_ring, 'E');  /* 缓冲区已满，应该被丢弃 */

    assert(ring_buf_count(&test_ring) == 4);
    assert(ring_buf_get(&test_ring) == 'A');
    assert(ring_buf_get(&test_ring) == 'B');
    assert(ring_buf_get(&test_ring) == 'C');
    assert(ring_buf_get(&test_ring) == 'D');
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

void test_ring_buf_wrap_around(void)
{
    printf("测试环形缓冲区环绕...\n");

    ring_buf_t test_ring;
    char test_buf[8];
    test_ring.head = 0;
    test_ring.tail = 0;
    test_ring.count = 0;
    test_ring.size = 8;
    test_ring.buf = test_buf;

    printf("  初始状态: count=%d, head=%d, tail=%d\n", test_ring.count, test_ring.head, test_ring.tail);

    /* 写入 3 个字符 */
    ring_buf_put(&test_ring, 'A');
    printf("  写入 A 后: count=%d, head=%d, tail=%d\n", test_ring.count, test_ring.head, test_ring.tail);
    ring_buf_put(&test_ring, 'B');
    printf("  写入 B 后: count=%d, head=%d, tail=%d\n", test_ring.count, test_ring.head, test_ring.tail);
    ring_buf_put(&test_ring, 'C');
    printf("  写入 C 后: count=%d, head=%d, tail=%d\n", test_ring.count, test_ring.head, test_ring.tail);

    printf("  写入后: count=%d, head=%d, tail=%d\n", test_ring.count, test_ring.head, test_ring.tail);

    /* 读取 2 个字符 */
    printf("  读取 A: %c\n", ring_buf_get(&test_ring));
    printf("  读取 B: %c\n", ring_buf_get(&test_ring));

    printf("  读取后: count=%d, head=%d, tail=%d\n", test_ring.count, test_ring.head, test_ring.tail);

    /* 写入 2 个字符，触发环绕 */
    ring_buf_put(&test_ring, 'D');
    printf("  写入 D 后: count=%d, head=%d, tail=%d\n", test_ring.count, test_ring.head, test_ring.tail);
    ring_buf_put(&test_ring, 'E');
    printf("  写入 E 后: count=%d, head=%d, tail=%d\n", test_ring.count, test_ring.head, test_ring.tail);

    printf("  再次写入后: count=%d, head=%d, tail=%d\n", test_ring.count, test_ring.head, test_ring.tail);

    printf("  断言 count == 3, 实际 count = %d\n", ring_buf_count(&test_ring));
    assert(ring_buf_count(&test_ring) == 3);
    printf("  断言读取 C\n");
    assert(ring_buf_get(&test_ring) == 'C');
    printf("  断言读取 D\n");
    assert(ring_buf_get(&test_ring) == 'D');
    printf("  断言读取 E\n");
    assert(ring_buf_get(&test_ring) == 'E');

    printf("  ✓ 测试通过\n");
}

int main(void)
{
    printf("=== 环形缓冲区单元测试 ===\n\n");

    test_ring_buf_put_get();
    test_ring_buf_overflow();
    test_ring_buf_empty();

    printf("\n=== 简单测试 ===\n");
    ring_buf_t test_ring;
    char test_buf[8];
    test_ring.head = 0;
    test_ring.tail = 0;
    test_ring.count = 0;
    test_ring.size = 8;
    test_ring.buf = test_buf;

    printf("初始: count=%d\n", ring_buf_count(&test_ring));
    ring_buf_put(&test_ring, 'A');
    printf("写入 A: count=%d\n", ring_buf_count(&test_ring));
    ring_buf_put(&test_ring, 'B');
    printf("写入 B: count=%d\n", ring_buf_count(&test_ring));
    ring_buf_put(&test_ring, 'C');
    printf("写入 C: count=%d\n", ring_buf_count(&test_ring));

    char c1 = ring_buf_get(&test_ring);
    printf("读取: %c, count=%d\n", c1, ring_buf_count(&test_ring));
    char c2 = ring_buf_get(&test_ring);
    printf("读取: %c, count=%d\n", c2, ring_buf_count(&test_ring));

    ring_buf_put(&test_ring, 'D');
    printf("写入 D: count=%d\n", ring_buf_count(&test_ring));
    ring_buf_put(&test_ring, 'E');
    printf("写入 E: count=%d\n", ring_buf_count(&test_ring));

    printf("最终: count=%d\n", ring_buf_count(&test_ring));

    test_ring_buf_wrap_around();

    printf("\n=== 所有测试通过 ===\n");
    return 0;
}
