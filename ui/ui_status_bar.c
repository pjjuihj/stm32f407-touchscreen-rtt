#include "ui_status_bar.h"
#include "ui_notification.h"
#include <stdio.h>

/* 回调函数 */
static void status_bar_press_cb(lv_event_t * e);
static void status_bar_release_cb(lv_event_t * e);

/* 静态变量 */
static lv_coord_t press_y = 0;

/**
 * @brief 创建状态栏组件
 */
lv_obj_t * ui_status_bar_create(lv_obj_t * parent)
{
    /* 创建状态栏容器 */
    lv_obj_t * status_bar = lv_obj_create(parent);
    if(status_bar == NULL) return NULL;

    /* 隐藏滚动条 */
    lv_obj_set_scrollbar_mode(status_bar, LV_SCROLLBAR_MODE_OFF);

    /* 设置状态栏大小和样式 */
    lv_obj_set_size(status_bar, LV_PCT(100), 30);
    lv_obj_set_style_bg_color(status_bar, lv_color_hex(0xF5F5F5), 0);
    lv_obj_set_style_bg_opa(status_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(status_bar, 0, 0);
    lv_obj_set_style_pad_all(status_bar, 5, 0);

    /* 确保状态栏可以接收点击事件 */
    lv_obj_add_flag(status_bar, LV_OBJ_FLAG_CLICKABLE);

    /* 添加按下和释放事件，用于检测下拉手势 */
    lv_obj_add_event_cb(status_bar, status_bar_press_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(status_bar, status_bar_release_cb, LV_EVENT_RELEASED, NULL);

    /* 创建时间标签 */
    lv_obj_t * time_label = lv_label_create(status_bar);
    if(time_label != NULL)
    {
        lv_label_set_text(time_label, "12:30");
        lv_obj_set_style_text_font(time_label, &lv_font_montserrat_12, 0);
        lv_obj_align(time_label, LV_ALIGN_LEFT_MID, 0, 0);
    }

    /* 创建电池图标 */
    lv_obj_t * battery_label = lv_label_create(status_bar);
    if(battery_label != NULL)
    {
        lv_label_set_text(battery_label, LV_SYMBOL_BATTERY_FULL " 85%");
        lv_obj_set_style_text_font(battery_label, &lv_font_montserrat_12, 0);
        lv_obj_align(battery_label, LV_ALIGN_RIGHT_MID, -60, 0);
    }

    /* 创建信号图标 */
    lv_obj_t * signal_label = lv_label_create(status_bar);
    if(signal_label != NULL)
    {
        lv_label_set_text(signal_label, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_font(signal_label, &lv_font_montserrat_12, 0);
        lv_obj_align(signal_label, LV_ALIGN_RIGHT_MID, -30, 0);
    }

    /* 创建通知图标 */
    lv_obj_t * notification_label = lv_label_create(status_bar);
    if(notification_label != NULL)
    {
        lv_label_set_text(notification_label, LV_SYMBOL_BELL);
        lv_obj_set_style_text_font(notification_label, &lv_font_montserrat_12, 0);
        lv_obj_align(notification_label, LV_ALIGN_RIGHT_MID, 0, 0);
    }

    return status_bar;
}

/**
 * @brief 状态栏按下回调，记录按下位置
 */
static void status_bar_press_cb(lv_event_t * e)
{
    lv_indev_t * indev = lv_indev_get_act();
    if(indev != NULL)
    {
        lv_point_t point;
        lv_indev_get_point(indev, &point);
        press_y = point.y;
        LV_LOG_USER("Status bar pressed at y=%d", press_y);
    }
    else
    {
        LV_LOG_ERROR("indev is NULL in press callback");
    }
}

/**
 * @brief 状态栏释放回调，检测下拉手势
 */
static void status_bar_release_cb(lv_event_t * e)
{
    lv_indev_t * indev = lv_indev_get_act();
    if(indev != NULL)
    {
        lv_point_t point;
        lv_indev_get_point(indev, &point);
        lv_coord_t release_y = point.y;

        /* 计算垂直位移 */
        lv_coord_t delta_y = release_y - press_y;
        LV_LOG_USER("Status bar released at y=%d, delta_y=%d", release_y, delta_y);

        /* 如果向下拖拽超过 20 像素，打开通知中心 */
        if(delta_y > 20)
        {
            LV_LOG_USER("Pull-down detected, showing notification center");
            ui_notification_show();
        }
        else
        {
            LV_LOG_USER("Not a pull-down, delta_y=%d", delta_y);
        }
    }
    else
    {
        LV_LOG_ERROR("indev is NULL in release callback");
    }
}
