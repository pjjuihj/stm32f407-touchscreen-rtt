/**
 * @file test_main.c
 * @brief 主屏幕组件单元测试
 *
 * 使用 LVGL 内置断言函数测试 ui_main_create() 函数
 */

#include <stdio.h>
#include <stdlib.h>

/* 包含 LVGL 和主屏幕头文件 */
#include "lvgl.h"
#include "ui.h"
#include "ui_swipe.h"

/* 测试计数器 */
static int tests_passed = 0;
static int tests_failed = 0;

/**
 * @brief 测试断言宏
 */
#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            printf("  PASS: %s\n", message); \
            tests_passed++; \
        } else { \
            printf("  FAIL: %s\n", message); \
            tests_failed++; \
        } \
    } while(0)

/**
 * @brief 测试 ui_main_create() 返回值不为 NULL
 */
static void test_main_create_not_null(void)
{
    printf("Test: main_create_not_null\n");

    /* 创建主屏幕 */
    lv_obj_t * main_screen = ui_main_create(lv_screen_active());

    /* 验证返回值不为 NULL */
    TEST_ASSERT(main_screen != NULL, "ui_main_create() should return non-NULL");

    /* 清理 */
    if (main_screen != NULL) {
        lv_obj_del(main_screen);
    }
}

/**
 * @brief 测试主屏幕大小是否正确
 */
static void test_main_size(void)
{
    printf("Test: main_size\n");

    /* 创建主屏幕 */
    lv_obj_t * main_screen = ui_main_create(lv_screen_active());

    /* 验证主屏幕存在 */
    TEST_ASSERT(main_screen != NULL, "main_screen should not be NULL");

    if (main_screen != NULL) {
        /* 验证主屏幕宽度为 100% */
        lv_coord_t width = lv_obj_get_width(main_screen);
        TEST_ASSERT(width == lv_pct(100), "main_screen width should be 100%");

        /* 验证主屏幕高度为 100% */
        lv_coord_t height = lv_obj_get_height(main_screen);
        TEST_ASSERT(height == lv_pct(100), "main_screen height should be 100%");

        /* 清理 */
        lv_obj_del(main_screen);
    }
}

/**
 * @brief 测试主屏幕背景颜色
 */
static void test_main_background_color(void)
{
    printf("Test: main_background_color\n");

    /* 创建主屏幕 */
    lv_obj_t * main_screen = ui_main_create(lv_screen_active());

    /* 验证主屏幕存在 */
    TEST_ASSERT(main_screen != NULL, "main_screen should not be NULL");

    if (main_screen != NULL) {
        /* 获取背景颜色 */
        lv_style_value_t bg_color = lv_obj_get_style_bg_color(main_screen, 0);

        /* 验证背景颜色为白色 (#FFFFFF) */
        TEST_ASSERT(bg_color.color.blue == 0xFF, "Blue component should be 0xFF");
        TEST_ASSERT(bg_color.color.green == 0xFF, "Green component should be 0xFF");
        TEST_ASSERT(bg_color.color.red == 0xFF, "Red component should be 0xFF");

        /* 清理 */
        lv_obj_del(main_screen);
    }
}

/**
 * @brief 测试主屏幕是否包含所有组件
 */
static void test_main_children(void)
{
    printf("Test: main_children\n");

    /* 创建主屏幕 */
    lv_obj_t * main_screen = ui_main_create(lv_screen_active());

    /* 验证主屏幕存在 */
    TEST_ASSERT(main_screen != NULL, "main_screen should not be NULL");

    if (main_screen != NULL) {
        /* 验证主屏幕有多个子控件（状态栏、应用网格、底部导航栏、通知中心） */
        uint32_t child_count = lv_obj_get_child_count(main_screen);
        TEST_ASSERT(child_count > 0, "main_screen should have children");

        /* 清理 */
        lv_obj_del(main_screen);
    }
}

/**
 * @brief 测试主屏幕创建多个实例
 */
static void test_main_multiple_instances(void)
{
    printf("Test: main_multiple_instances\n");

    /* 创建多个主屏幕 */
    lv_obj_t * main_screen1 = ui_main_create(lv_screen_active());
    lv_obj_t * main_screen2 = ui_main_create(lv_screen_active());

    /* 验证所有主屏幕都不为 NULL */
    TEST_ASSERT(main_screen1 != NULL, "main_screen1 should not be NULL");
    TEST_ASSERT(main_screen2 != NULL, "main_screen2 should not be NULL");

    /* 清理 */
    if (main_screen1 != NULL) lv_obj_del(main_screen1);
    if (main_screen2 != NULL) lv_obj_del(main_screen2);
}

/**
 * @brief 主函数，运行所有测试
 */
int main(void)
{
    printf("=== Main Screen Unit Tests ===\n\n");

    /* 初始化 LVGL */
    lv_init();

    /* 运行所有测试 */
    test_main_create_not_null();
    test_main_size();
    test_main_background_color();
    test_main_children();
    test_main_multiple_instances();

    /* 清理 LVGL */
    lv_deinit();

    /* 输出测试结果 */
    printf("\n=== Test Results ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    printf("Total: %d\n", tests_passed + tests_failed);

    /* 返回测试结果 */
    return (tests_failed == 0) ? 0 : 1;
}
