/**
 * @file test_status_bar.c
 * @brief 状态栏组件单元测试
 *
 * 使用 LVGL 内置断言函数测试 ui_status_bar_create() 函数
 */

#include <stdio.h>
#include <stdlib.h>

/* 包含 LVGL 和状态栏头文件 */
#include "lvgl.h"
#include "ui_status_bar.h"

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
 * @brief 测试 ui_status_bar_create() 返回值不为 NULL
 */
static void test_status_bar_create_not_null(void)
{
    printf("Test: status_bar_create_not_null\n");

    /* 创建状态栏 */
    lv_obj_t * status_bar = ui_status_bar_create(lv_screen_active());

    /* 验证返回值不为 NULL */
    TEST_ASSERT(status_bar != NULL, "ui_status_bar_create() should return non-NULL");

    /* 清理 */
    if (status_bar != NULL) {
        lv_obj_del(status_bar);
    }
}

/**
 * @brief 测试状态栏大小是否正确
 */
static void test_status_bar_size(void)
{
    printf("Test: status_bar_size\n");

    /* 创建状态栏 */
    lv_obj_t * status_bar = ui_status_bar_create(lv_screen_active());

    /* 验证状态栏存在 */
    TEST_ASSERT(status_bar != NULL, "status_bar should not be NULL");

    if (status_bar != NULL) {
        /* 验证状态栏高度为 30 */
        lv_coord_t height = lv_obj_get_height(status_bar);
        TEST_ASSERT(height == 30, "status_bar height should be 30");

        /* 清理 */
        lv_obj_del(status_bar);
    }
}

/**
 * @brief 测试状态栏背景颜色
 */
static void test_status_bar_background_color(void)
{
    printf("Test: status_bar_background_color\n");

    /* 创建状态栏 */
    lv_obj_t * status_bar = ui_status_bar_create(lv_screen_active());

    /* 验证状态栏存在 */
    TEST_ASSERT(status_bar != NULL, "status_bar should not be NULL");

    if (status_bar != NULL) {
        /* 获取背景颜色 */
        lv_style_value_t bg_color = lv_obj_get_style_bg_color(status_bar, 0);

        /* 验证背景颜色为浅灰色 (#F5F5F5) */
        TEST_ASSERT(bg_color.color.blue == 0xF5, "Blue component should be 0xF5");
        TEST_ASSERT(bg_color.color.green == 0xF5, "Green component should be 0xF5");
        TEST_ASSERT(bg_color.color.red == 0xF5, "Red component should be 0xF5");

        /* 清理 */
        lv_obj_del(status_bar);
    }
}

/**
 * @brief 测试状态栏是否包含所有子控件
 */
static void test_status_bar_children(void)
{
    printf("Test: status_bar_children\n");

    /* 创建状态栏 */
    lv_obj_t * status_bar = ui_status_bar_create(lv_screen_active());

    /* 验证状态栏存在 */
    TEST_ASSERT(status_bar != NULL, "status_bar should not be NULL");

    if (status_bar != NULL) {
        /* 验证状态栏有 4 个子控件（时间、电池、信号、通知） */
        uint32_t child_count = lv_obj_get_child_count(status_bar);
        TEST_ASSERT(child_count == 4, "status_bar should have 4 children");

        /* 清理 */
        lv_obj_del(status_bar);
    }
}

/**
 * @brief 测试状态栏创建多个实例
 */
static void test_status_bar_multiple_instances(void)
{
    printf("Test: status_bar_multiple_instances\n");

    /* 创建多个状态栏 */
    lv_obj_t * status_bar1 = ui_status_bar_create(lv_screen_active());
    lv_obj_t * status_bar2 = ui_status_bar_create(lv_screen_active());
    lv_obj_t * status_bar3 = ui_status_bar_create(lv_screen_active());

    /* 验证所有状态栏都不为 NULL */
    TEST_ASSERT(status_bar1 != NULL, "status_bar1 should not be NULL");
    TEST_ASSERT(status_bar2 != NULL, "status_bar2 should not be NULL");
    TEST_ASSERT(status_bar3 != NULL, "status_bar3 should not be NULL");

    /* 验证所有状态栏都有 4 个子控件 */
    if (status_bar1 != NULL) {
        TEST_ASSERT(lv_obj_get_child_count(status_bar1) == 4, "status_bar1 should have 4 children");
    }
    if (status_bar2 != NULL) {
        TEST_ASSERT(lv_obj_get_child_count(status_bar2) == 4, "status_bar2 should have 4 children");
    }
    if (status_bar3 != NULL) {
        TEST_ASSERT(lv_obj_get_child_count(status_bar3) == 4, "status_bar3 should have 4 children");
    }

    /* 清理 */
    if (status_bar1 != NULL) lv_obj_del(status_bar1);
    if (status_bar2 != NULL) lv_obj_del(status_bar2);
    if (status_bar3 != NULL) lv_obj_del(status_bar3);
}

/**
 * @brief 主函数，运行所有测试
 */
int main(void)
{
    printf("=== Status Bar Unit Tests ===\n\n");

    /* 初始化 LVGL */
    lv_init();

    /* 运行所有测试 */
    test_status_bar_create_not_null();
    test_status_bar_size();
    test_status_bar_background_color();
    test_status_bar_children();
    test_status_bar_multiple_instances();

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
