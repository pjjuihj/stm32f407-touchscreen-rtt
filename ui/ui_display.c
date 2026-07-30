#include "ui.h"
#include <stdio.h>

/* 控件对象 */
static lv_obj_t * bar = NULL;
static lv_obj_t * bar_value_label = NULL;
static lv_obj_t * slider = NULL;
static lv_obj_t * led1 = NULL;
static lv_obj_t * led2 = NULL;
static lv_obj_t * led3 = NULL;

/* 回调函数 */
static void slider_event_cb(lv_event_t * e);
static void led1_event_cb(lv_event_t * e);
static void led2_event_cb(lv_event_t * e);
static void led3_event_cb(lv_event_t * e);

/**
 * @brief 初始化显示控件页面
 * @param parent 父对象（tileview tile）
 */
lv_obj_t * ui_display_create(lv_obj_t * parent)
{
    /* 创建容器 */
    lv_obj_t * scr = lv_obj_create(parent);

    /* 设置容器尺寸和样式 */
    lv_obj_set_size(scr, 240, 320);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* 创建标题标签 */
    lv_obj_t * title = lv_label_create(scr);
    if(title != NULL)
    {
        lv_label_set_text(title, "Display Controls");
        lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    }

    /* 创建滚动容器 */
    lv_obj_t * container = lv_obj_create(scr);
    if(container != NULL)
    {
        lv_obj_set_size(container, 230, 280);
        lv_obj_align(container, LV_ALIGN_TOP_MID, 0, 35);
        lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(container, 0, 0);
        lv_obj_set_style_pad_all(container, 0, 0);
        lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(container, 10, 0);

        /* Bar 标签 */
        lv_obj_t * bar_title = lv_label_create(container);
        if(bar_title != NULL)
        {
            lv_label_set_text(bar_title, "Progress Bar:");
            lv_obj_set_style_text_font(bar_title, &lv_font_montserrat_14, 0);
            lv_obj_set_width(bar_title, 200);
        }

        /* Bar 控件 */
        bar = lv_bar_create(container);
        if(bar != NULL)
        {
            lv_obj_set_width(bar, 200);
            lv_obj_set_height(bar, 20);
            lv_bar_set_range(bar, 0, 100);
            lv_bar_set_value(bar, 50, LV_ANIM_ON);
        }

        /* Bar 值标签 */
        bar_value_label = lv_label_create(container);
        if(bar_value_label != NULL)
        {
            lv_label_set_text(bar_value_label, "50%");
            lv_obj_set_style_text_font(bar_value_label, &lv_font_montserrat_14, 0);
        }

        /* Slider 标签 */
        lv_obj_t * slider_title = lv_label_create(container);
        if(slider_title != NULL)
        {
            lv_label_set_text(slider_title, "Slider Control:");
            lv_obj_set_style_text_font(slider_title, &lv_font_montserrat_14, 0);
            lv_obj_set_width(slider_title, 200);
        }

        /* Slider 控件 */
        slider = lv_slider_create(container);
        if(slider != NULL)
        {
            lv_obj_set_width(slider, 200);
            lv_slider_set_range(slider, 0, 100);
            lv_slider_set_value(slider, 50, LV_ANIM_OFF);
            lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
        }

        /* Spinner 标签 */
        lv_obj_t * spinner_title = lv_label_create(container);
        if(spinner_title != NULL)
        {
            lv_label_set_text(spinner_title, "Spinner:");
            lv_obj_set_style_text_font(spinner_title, &lv_font_montserrat_14, 0);
            lv_obj_set_width(spinner_title, 200);
        }

        /* Spinner 控件 */
        lv_obj_t * spinner = lv_spinner_create(container);
        if(spinner != NULL)
        {
            lv_obj_set_size(spinner, 40, 40);
        }

        /* LED 标签 */
        lv_obj_t * led_title = lv_label_create(container);
        if(led_title != NULL)
        {
            lv_label_set_text(led_title, "LED:");
            lv_obj_set_style_text_font(led_title, &lv_font_montserrat_14, 0);
            lv_obj_set_width(led_title, 200);
        }

        /* LED 容器 */
        lv_obj_t * led_container = lv_obj_create(container);
        if(led_container != NULL)
        {
            lv_obj_set_size(led_container, 200, 40);
            lv_obj_set_style_bg_opa(led_container, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(led_container, 0, 0);
            lv_obj_set_style_pad_all(led_container, 0, 0);
            lv_obj_set_flex_flow(led_container, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(led_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

            /* LED 1 */
            led1 = lv_led_create(led_container);
            if(led1 != NULL)
            {
                lv_obj_set_size(led1, 30, 30);
                lv_led_set_color(led1, lv_color_hex(0x00FF00));
                lv_led_on(led1);
                lv_obj_add_event_cb(led1, led1_event_cb, LV_EVENT_CLICKED, NULL);
            }

            /* LED 2 */
            led2 = lv_led_create(led_container);
            if(led2 != NULL)
            {
                lv_obj_set_size(led2, 30, 30);
                lv_led_set_color(led2, lv_color_hex(0x00FF00));
                lv_led_off(led2);
                lv_obj_add_event_cb(led2, led2_event_cb, LV_EVENT_CLICKED, NULL);
            }

            /* LED 3 */
            led3 = lv_led_create(led_container);
            if(led3 != NULL)
            {
                lv_obj_set_size(led3, 30, 30);
                lv_led_set_color(led3, lv_color_hex(0x00FF00));
                lv_led_on(led3);
                lv_obj_add_event_cb(led3, led3_event_cb, LV_EVENT_CLICKED, NULL);
            }
        }
    }

    return scr;
}

/* 滑块值变化回调 */
static void slider_event_cb(lv_event_t * e)
{
    lv_obj_t * obj = lv_event_get_target(e);
    int32_t value = lv_slider_get_value(obj);

    /* 更新进度条 */
    if(bar != NULL)
    {
        lv_bar_set_value(bar, value, LV_ANIM_ON);
    }

    /* 更新进度条值标签 */
    if(bar_value_label != NULL)
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%ld%%", value);
        lv_label_set_text(bar_value_label, buf);
    }

    LV_LOG_USER("Slider value: %ld", value);
}

/* LED 1 点击回调 */
static void led1_event_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    lv_led_toggle(led1);
    if(lv_led_get_brightness(led1) > LV_LED_BRIGHT_MIN)
    {
        LV_LOG_USER("LED1 ON");
    }
    else
    {
        LV_LOG_USER("LED1 OFF");
    }
}

/* LED 2 点击回调 */
static void led2_event_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    lv_led_toggle(led2);
    if(lv_led_get_brightness(led2) > LV_LED_BRIGHT_MIN)
    {
        LV_LOG_USER("LED2 ON");
    }
    else
    {
        LV_LOG_USER("LED2 OFF");
    }
}

/* LED 3 点击回调 */
static void led3_event_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    lv_led_toggle(led3);
    if(lv_led_get_brightness(led3) > LV_LED_BRIGHT_MIN)
    {
        LV_LOG_USER("LED3 ON");
    }
    else
    {
        LV_LOG_USER("LED3 OFF");
    }
}

