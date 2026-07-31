/**
 * @file test_bottom_nav.c
 * @brief 底部导航栏组件单元测试
 *
 * 使用 LVGL 内置断言函数测试 ui_bottom_nav_create() 函数
 */

#include <stdio.h>
#include <stdlib.h>

/* 包含 LVGL 和底部导航栏头文件 */
#include "lvgl.h"
#include "ui_bottom_nav.h"

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
 * @brief 测试 ui_bottom_nav_create() 返回值不为 NULL
 */
static void test_bottom_nav_create_not_null(void)
{
    printf("Test: bottom_nav_create_not_null\n");

    /* 创建底部导航栏 */
    lv_obj_t * nav_bar = ui_bottom_nav_create(lv_screen_active());

    /* 验证返回值不为 NULL */
    TEST_ASSERT(nav_bar != NULL, "ui_bottom_nav_create() should return non-NULL");

    /* 清理 */
    if (nav_bar != NULL) {
        lv_obj_del(nav_bar);
    }
}

/**
 * @brief 测试底部导航栏大小是否正确
 */
static void test_bottom_nav_size(void)
{
    printf("Test: bottom_nav_size\n");

    /* 创建底部导航栏 */
    lv_obj_t * nav_bar = ui_bottom_nav_create(lv_screen_active());

    /* 验证底部导航栏存在 */
    TEST_ASSERT(nav_bar != NULL, "nav_bar should not be NULL");

    if (nav_bar != NULL) {
        /* 验证底部导航栏高度为 50 */
        lv_coord_t height = lv_obj_get_height(nav_bar);
        TEST_ASSERT(height == 50, "nav_bar height should be 50");

        /* 清理 */
        lv_obj_del(nav_bar);
    }
}

/**
 * @brief 测试底部导航栏背景颜色
 */
static void test_bottom_nav_background_color(void)
{
    printf("Test: bottom_nav_background_color\n");

    /* 创建底部导航栏 */
    lv_obj_t * nav_bar = ui_bottom_nav_create(lv_screen_active());

    /* 验证底部导航栏存在 */
    TEST_ASSERT(nav_bar != NULL, "nav_bar should not be NULL");

    if (nav_bar != NULL) {
        /* 获取背景颜色 */
        lv_style_value_t bg_color = lv_obj_get_style_bg_color(nav_bar, 0);

        /* 验证背景颜色为白色 (#FFFFFF) */
        TEST_ASSERT(bg_color.color.blue == 0xFF, "Blue component should be 0xFF");
        TEST_ASSERT(bg_color.color.green == 0xFF, "Green component should be 0xFF");
        TEST_ASSERT(bg_color.color.red == 0xFF, "Red component should be 0xFF");

        /* 清理 */
        lv_obj_del(nav_bar);
    }
}

/**
 * @brief 测试底部导航栏是否包含所有按钮
 */
static void test_bottom_nav_children(void)
{
    printf("Test: bottom_nav_children\n");

    /* 创建底部导航栏 */
    lv_obj_t * nav_bar = ui_bottom_nav_create(lv_screen_active());

    /* 验证底部导航栏存在 */
    TEST_ASSERT(nav_bar != NULL, "nav_bar should not be NULL");

    if (nav_bar != NULL) {
        /* 验证底部导航栏有 4 个子控件（返回、主页、最近任务、通知） */
        uint32_t child_count = lv_obj_get_child_count(nav_bar);
        TEST_ASSERT(child_count == 4, "nav_bar should have 4 children");

        /* 清理 */
        lv_obj_del(nav_bar);
    }
}

/**
 * @brief 测试底部导航栏设置活跃按钮
 */
static void test_bottom_nav_set_active(void)
{
    printf("Test: bottom_nav_set_active\n");

    /* 创建底部导航栏 */
    lv_obj_t * nav_bar = ui_bottom_nav_create(lv_screen_active());

    /* 验证底部导航栏存在 */
    TEST_ASSERT(nav_bar != NULL, "nav_bar should not be NULL");

    if (nav_bar != NULL) {
        /* 测试设置不同的活跃按钮 */
        ui_bottom_nav_set_active(BOTTOM_NAV_BACK);
        ui_bottom_nav_set_active(BOTTOM_NAV_HOME);
        ui_bottom_nav_set_active(BOTTOM_NAV_RECENT);
        ui_bottom_nav_set_active(BOTTOM_NAV_NOTIFY);

        /* 验证所有设置都成功（没有崩溃） */
        TEST_ASSERT(1, "ui_bottom_nav_set_active() should not crash");

        /* 清理 */
        lv_obj_del(nav_bar);
    }
}

/**
 * @brief 主函数，运行所有测试
 */
int main(void)
{
    printf("=== Bottom Navigation Bar Unit Tests ===\n\n");

    /* 初始化 LVGL */
    lv_init();

    /* 运行所有测试 */
    test_bottom_nav_create_not_null();
    test_bottom_nav_size();
    test_bottom_nav_background_color();
    test_bottom_nav_children();
    test_bottom_nav_set_active();

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
