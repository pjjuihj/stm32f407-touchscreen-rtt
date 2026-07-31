/**
 * @file test_lvgl_log.c
 * @brief LVGL 日志功能单元测试
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

/* 模拟 LVGL 日志级别 */
#define LV_LOG_LEVEL_TRACE 0
#define LV_LOG_LEVEL_INFO  1
#define LV_LOG_LEVEL_WARN  2
#define LV_LOG_LEVEL_ERROR 3
#define LV_LOG_LEVEL_USER  4

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

/* LVGL 日志回调函数（从 gui_driver.c 复制） */
static ring_buf_t lvgl_ring;
static char lvgl_buf[256];

static void lv_log_print_g_cb(int level, const char *buf)
{
    /* 递归保护 */
    static volatile bool in_log_cb = false;
    if(in_log_cb) return;
    in_log_cb = true;

    /* 日志级别前缀 */
    const char *prefix;
    switch(level) {
        case LV_LOG_LEVEL_TRACE: prefix = "[T] "; break;
        case LV_LOG_LEVEL_INFO:  prefix = "[I] "; break;
        case LV_LOG_LEVEL_WARN:  prefix = "[W] "; break;
        case LV_LOG_LEVEL_ERROR: prefix = "[E] "; break;
        case LV_LOG_LEVEL_USER:  prefix = "[U] "; break;
        default:                 prefix = "[?] "; break;
    }

    /* 写入前缀 */
    for(const char *p = prefix; *p; p++) {
        ring_buf_put(&lvgl_ring, *p);
    }

    /* 写入日志内容 */
    for(const char *p = buf; *p; p++) {
        ring_buf_put(&lvgl_ring, *p);
    }

    /* 换行符 */
    ring_buf_put(&lvgl_ring, '\r');
    ring_buf_put(&lvgl_ring, '\n');

    in_log_cb = false;
}

/* 辅助函数：从环形缓冲区读取字符串 */
static void read_string(ring_buf_t *ring, char *out, int max_len)
{
    int i = 0;
    int c;
    while(i < max_len - 1 && (c = ring_buf_get(ring)) != -1) {
        out[i++] = (char)c;
    }
    out[i] = '\0';
}

/* 测试函数 */
void test_lvgl_log_info(void)
{
    printf("测试 LVGL INFO 日志...\n");

    /* 初始化 */
    lvgl_ring.head = 0;
    lvgl_ring.tail = 0;
    lvgl_ring.count = 0;
    lvgl_ring.size = 256;
    lvgl_ring.buf = lvgl_buf;

    /* 调用日志回调 */
    lv_log_print_g_cb(LV_LOG_LEVEL_INFO, "Test message");

    /* 验证输出 */
    char output[256];
    read_string(&lvgl_ring, output, sizeof(output));

    assert(strstr(output, "[I]") != NULL);
    assert(strstr(output, "Test message") != NULL);
    assert(strstr(output, "\r\n") != NULL);

    printf("  ✓ 测试通过: %s\n", output);
}

void test_lvgl_log_warn(void)
{
    printf("测试 LVGL WARN 日志...\n");

    /* 初始化 */
    lvgl_ring.head = 0;
    lvgl_ring.tail = 0;
    lvgl_ring.count = 0;
    lvgl_ring.size = 256;
    lvgl_ring.buf = lvgl_buf;

    /* 调用日志回调 */
    lv_log_print_g_cb(LV_LOG_LEVEL_WARN, "Warning message");

    /* 验证输出 */
    char output[256];
    read_string(&lvgl_ring, output, sizeof(output));

    assert(strstr(output, "[W]") != NULL);
    assert(strstr(output, "Warning message") != NULL);

    printf("  ✓ 测试通过: %s\n", output);
}

void test_lvgl_log_error(void)
{
    printf("测试 LVGL ERROR 日志...\n");

    /* 初始化 */
    lvgl_ring.head = 0;
    lvgl_ring.tail = 0;
    lvgl_ring.count = 0;
    lvgl_ring.size = 256;
    lvgl_ring.buf = lvgl_buf;

    /* 调用日志回调 */
    lv_log_print_g_cb(LV_LOG_LEVEL_ERROR, "Error occurred");

    /* 验证输出 */
    char output[256];
    read_string(&lvgl_ring, output, sizeof(output));

    assert(strstr(output, "[E]") != NULL);
    assert(strstr(output, "Error occurred") != NULL);

    printf("  ✓ 测试通过: %s\n", output);
}

void test_lvgl_log_trace(void)
{
    printf("测试 LVGL TRACE 日志...\n");

    /* 初始化 */
    lvgl_ring.head = 0;
    lvgl_ring.tail = 0;
    lvgl_ring.count = 0;
    lvgl_ring.size = 256;
    lvgl_ring.buf = lvgl_buf;

    /* 调用日志回调 */
    lv_log_print_g_cb(LV_LOG_LEVEL_TRACE, "Trace info");

    /* 验证输出 */
    char output[256];
    read_string(&lvgl_ring, output, sizeof(output));

    assert(strstr(output, "[T]") != NULL);
    assert(strstr(output, "Trace info") != NULL);

    printf("  ✓ 测试通过: %s\n", output);
}

void test_lvgl_log_user(void)
{
    printf("测试 LVGL USER 日志...\n");

    /* 初始化 */
    lvgl_ring.head = 0;
    lvgl_ring.tail = 0;
    lvgl_ring.count = 0;
    lvgl_ring.size = 256;
    lvgl_ring.buf = lvgl_buf;

    /* 调用日志回调 */
    lv_log_print_g_cb(LV_LOG_LEVEL_USER, "User message");

    /* 验证输出 */
    char output[256];
    read_string(&lvgl_ring, output, sizeof(output));

    assert(strstr(output, "[U]") != NULL);
    assert(strstr(output, "User message") != NULL);

    printf("  ✓ 测试通过: %s\n", output);
}

void test_lvgl_log_unknown_level(void)
{
    printf("测试 LVGL 未知日志级别...\n");

    /* 初始化 */
    lvgl_ring.head = 0;
    lvgl_ring.tail = 0;
    lvgl_ring.count = 0;
    lvgl_ring.size = 256;
    lvgl_ring.buf = lvgl_buf;

    /* 调用日志回调（使用未知级别） */
    lv_log_print_g_cb(99, "Unknown level");

    /* 验证输出 */
    char output[256];
    read_string(&lvgl_ring, output, sizeof(output));

    assert(strstr(output, "[?]") != NULL);
    assert(strstr(output, "Unknown level") != NULL);

    printf("  ✓ 测试通过: %s\n", output);
}

void test_lvgl_log_multiple_messages(void)
{
    printf("测试 LVGL 多条日志...\n");

    /* 初始化 */
    lvgl_ring.head = 0;
    lvgl_ring.tail = 0;
    lvgl_ring.count = 0;
    lvgl_ring.size = 256;
    lvgl_ring.buf = lvgl_buf;

    /* 写入多条日志 */
    lv_log_print_g_cb(LV_LOG_LEVEL_INFO, "Message 1");
    lv_log_print_g_cb(LV_LOG_LEVEL_WARN, "Message 2");
    lv_log_print_g_cb(LV_LOG_LEVEL_ERROR, "Message 3");

    /* 验证输出 */
    char output[512];
    read_string(&lvgl_ring, output, sizeof(output));

    assert(strstr(output, "[I] Message 1") != NULL);
    assert(strstr(output, "[W] Message 2") != NULL);
    assert(strstr(output, "[E] Message 3") != NULL);

    printf("  ✓ 测试通过\n");
}

void test_lvgl_log_overflow(void)
{
    printf("测试 LVGL 日志缓冲区溢出...\n");

    /* 初始化（使用小缓冲区） */
    lvgl_ring.head = 0;
    lvgl_ring.tail = 0;
    lvgl_ring.count = 0;
    lvgl_ring.size = 20;  /* 只有 20 字节 */
    lvgl_ring.buf = lvgl_buf;

    /* 写入长日志，应该被截断 */
    lv_log_print_g_cb(LV_LOG_LEVEL_INFO, "This is a very long message that should be truncated");

    /* 验证输出被截断 */
    assert(ring_buf_count(&lvgl_ring) <= 20);

    printf("  ✓ 测试通过: 缓冲区满时丢弃字符\n");
}

int main(void)
{
    printf("=== LVGL 日志功能单元测试 ===\n\n");

    test_lvgl_log_info();
    test_lvgl_log_warn();
    test_lvgl_log_error();
    test_lvgl_log_trace();
    test_lvgl_log_user();
    test_lvgl_log_unknown_level();
    test_lvgl_log_multiple_messages();
    test_lvgl_log_overflow();

    printf("\n=== 所有测试通过 ===\n");
    return 0;
}
