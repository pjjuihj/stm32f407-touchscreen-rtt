#include "ui.h"
#include <stdio.h>

/* 控件对象 */
static lv_obj_t * slider = NULL;
static lv_obj_t * slider_label = NULL;
static lv_obj_t * arc = NULL;
static lv_obj_t * arc_label = NULL;

/* 回调函数 */
static void slider_event_cb(lv_event_t * e);
static void arc_event_cb(lv_event_t * e);

/**
 * @brief 初始化输入控件页面
 * @param parent 父对象（tileview tile）
 */
lv_obj_t * ui_input_create(lv_obj_t * parent)
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
        lv_label_set_text(title, "Input Controls");
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

        /* Slider 标签 */
        lv_obj_t * slider_title = lv_label_create(container);
        if(slider_title != NULL)
        {
            lv_label_set_text(slider_title, "Slider:");
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

        /* Slider 值标签 */
        slider_label = lv_label_create(container);
        if(slider_label != NULL)
        {
            lv_label_set_text(slider_label, "50");
            lv_obj_set_style_text_font(slider_label, &lv_font_montserrat_14, 0);
        }

        /* Switch 标签 */
        lv_obj_t * switch_title = lv_label_create(container);
        if(switch_title != NULL)
        {
            lv_label_set_text(switch_title, "Switch:");
            lv_obj_set_style_text_font(switch_title, &lv_font_montserrat_14, 0);
            lv_obj_set_width(switch_title, 200);
        }

        /* Switch 控件 */
        lv_obj_t * sw = lv_switch_create(container);
        if(sw != NULL)
        {
            lv_obj_set_width(sw, 50);
            lv_obj_set_height(sw, 25);
        }

        /* Arc 标签 */
        lv_obj_t * arc_title = lv_label_create(container);
        if(arc_title != NULL)
        {
            lv_label_set_text(arc_title, "Arc:");
            lv_obj_set_style_text_font(arc_title, &lv_font_montserrat_14, 0);
            lv_obj_set_width(arc_title, 200);
        }

        /* Arc 控件 */
        arc = lv_arc_create(container);
        if(arc != NULL)
        {
            lv_obj_set_size(arc, 100, 100);
            lv_arc_set_range(arc, 0, 100);
            lv_arc_set_value(arc, 50);
            lv_obj_add_event_cb(arc, arc_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
        }

        /* Arc 值标签 */
        arc_label = lv_label_create(container);
        if(arc_label != NULL)
        {
            lv_label_set_text(arc_label, "50");
            lv_obj_set_style_text_font(arc_label, &lv_font_montserrat_14, 0);
        }
    }

    return scr;
}

/* Slider 值变化回调 */
static void slider_event_cb(lv_event_t * e)
{
    lv_obj_t * obj = lv_event_get_target(e);
    int32_t value = lv_slider_get_value(obj);
    char buf[16];
    snprintf(buf, sizeof(buf), "%ld", value);
    lv_label_set_text(slider_label, buf);
    LV_LOG_USER("Slider value: %ld", value);
}

/* Arc 值变化回调 */
static void arc_event_cb(lv_event_t * e)
{
    lv_obj_t * obj = lv_event_get_target(e);
    int32_t value = lv_arc_get_value(obj);
    char buf[16];
    snprintf(buf, sizeof(buf), "%ld", value);
    lv_label_set_text(arc_label, buf);
    LV_LOG_USER("Arc value: %ld", value);
}
