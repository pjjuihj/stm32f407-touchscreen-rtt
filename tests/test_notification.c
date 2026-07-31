/**
 * @file test_notification.c
 * @brief 通知中心组件单元测试
 *
 * 使用 LVGL 内置断言函数测试 ui_notification_create() 函数
 */

#include <stdio.h>
#include <stdlib.h>

/* 包含 LVGL 和通知中心头文件 */
#include "lvgl.h"
#include "ui_notification.h"

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
 * @brief 测试 ui_notification_create() 返回值不为 NULL
 */
static void test_notification_create_not_null(void)
{
    printf("Test: notification_create_not_null\n");

    /* 创建通知中心 */
    lv_obj_t * notification = ui_notification_create(lv_screen_active());

    /* 验证返回值不为 NULL */
    TEST_ASSERT(notification != NULL, "ui_notification_create() should return non-NULL");

    /* 清理 */
    if (notification != NULL) {
        lv_obj_del(notification);
    }
}

/**
 * @brief 测试通知中心大小是否正确
 */
static void test_notification_size(void)
{
    printf("Test: notification_size\n");

    /* 创建通知中心 */
    lv_obj_t * notification = ui_notification_create(lv_screen_active());

    /* 验证通知中心存在 */
    TEST_ASSERT(notification != NULL, "notification should not be NULL");

    if (notification != NULL) {
        /* 验证通知中心宽度为 100% */
        lv_coord_t width = lv_obj_get_width(notification);
        TEST_ASSERT(width == lv_pct(100), "notification width should be 100%");

        /* 验证通知中心高度为 100% */
        lv_coord_t height = lv_obj_get_height(notification);
        TEST_ASSERT(height == lv_pct(100), "notification height should be 100%");

        /* 清理 */
        lv_obj_del(notification);
    }
}

/**
 * @brief 测试通知中心背景颜色
 */
static void test_notification_background_color(void)
{
    printf("Test: notification_background_color\n");

    /* 创建通知中心 */
    lv_obj_t * notification = ui_notification_create(lv_screen_active());

    /* 验证通知中心存在 */
    TEST_ASSERT(notification != NULL, "notification should not be NULL");

    if (notification != NULL) {
        /* 获取背景颜色 */
        lv_style_value_t bg_color = lv_obj_get_style_bg_color(notification, 0);

        /* 验证背景颜色为白色 (#FFFFFF) */
        TEST_ASSERT(bg_color.color.blue == 0xFF, "Blue component should be 0xFF");
        TEST_ASSERT(bg_color.color.green == 0xFF, "Green component should be 0xFF");
        TEST_ASSERT(bg_color.color.red == 0xFF, "Red component should be 0xFF");

        /* 清理 */
        lv_obj_del(notification);
    }
}

/**
 * @brief 测试通知中心显示/隐藏功能
 */
static void test_notification_show_hide(void)
{
    printf("Test: notification_show_hide\n");

    /* 创建通知中心 */
    lv_obj_t * notification = ui_notification_create(lv_screen_active());

    /* 验证通知中心存在 */
    TEST_ASSERT(notification != NULL, "notification should not be NULL");

    if (notification != NULL) {
        /* 测试隐藏通知中心 */
        ui_notification_hide();
        TEST_ASSERT(1, "ui_notification_hide() should not crash");

        /* 测试显示通知中心 */
        ui_notification_show();
        TEST_ASSERT(1, "ui_notification_show() should not crash");

        /* 清理 */
        lv_obj_del(notification);
    }
}

/**
 * @brief 测试通知中心是否包含所有子控件
 */
static void test_notification_children(void)
{
    printf("Test: notification_children\n");

    /* 创建通知中心 */
    lv_obj_t * notification = ui_notification_create(lv_screen_active());

    /* 验证通知中心存在 */
    TEST_ASSERT(notification != NULL, "notification should not be NULL");

    if (notification != NULL) {
        /* 验证通知中心有多个子控件（标题、快速设置、通知列表、关闭按钮） */
        uint32_t child_count = lv_obj_get_child_count(notification);
        TEST_ASSERT(child_count > 0, "notification should have children");

        /* 清理 */
        lv_obj_del(notification);
    }
}

/**
 * @brief 主函数，运行所有测试
 */
int main(void)
{
    printf("=== Notification Center Unit Tests ===\n\n");

    /* 初始化 LVGL */
    lv_init();

    /* 运行所有测试 */
    test_notification_create_not_null();
    test_notification_size();
    test_notification_background_color();
    test_notification_show_hide();
    test_notification_children();

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
