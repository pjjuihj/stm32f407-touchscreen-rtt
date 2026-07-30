#include "ui.h"

/* 回调函数 */
static void back_btn_cb(lv_event_t * e);
static void dropdown_event_cb(lv_event_t * e);
static void roller_event_cb(lv_event_t * e);

/**
 * @brief 初始化选择控件页面
 */
lv_obj_t * ui_selection_create(void)
{
    /* 创建新屏幕 */
    lv_obj_t * scr = lv_obj_create(NULL);

    /* 设置屏幕背景色 */
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* 创建返回按钮 */
    lv_obj_t * back_btn = lv_btn_create(scr);
    if(back_btn != NULL)
    {
        lv_obj_set_size(back_btn, 40, 30);
        lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 5, 5);
        lv_obj_add_event_cb(back_btn, back_btn_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t * back_label = lv_label_create(back_btn);
        if(back_label != NULL)
        {
            lv_label_set_text(back_label, LV_SYMBOL_LEFT " 返回");
            lv_obj_center(back_label);
        }
    }

    /* 创建标题标签 */
    lv_obj_t * title = lv_label_create(scr);
    if(title != NULL)
    {
        lv_label_set_text(title, "选择控件");
        lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    }

    /* 创建滚动容器 */
    lv_obj_t * container = lv_obj_create(scr);
    if(container != NULL)
    {
        lv_obj_set_size(container, 230, 260);
        lv_obj_align(container, LV_ALIGN_TOP_MID, 0, 35);
        lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(container, 0, 0);
        lv_obj_set_style_pad_all(container, 0, 0);
        lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(container, 10, 0);

        /* Dropdown 标签 */
        lv_obj_t * dropdown_title = lv_label_create(container);
        if(dropdown_title != NULL)
        {
            lv_label_set_text(dropdown_title, "Dropdown:");
            lv_obj_set_style_text_font(dropdown_title, &lv_font_montserrat_14, 0);
            lv_obj_set_width(dropdown_title, 200);
        }

        /* Dropdown 控件 */
        lv_obj_t * dropdown = lv_dropdown_create(container);
        if(dropdown != NULL)
        {
            lv_obj_set_width(dropdown, 200);
            lv_dropdown_set_options(dropdown, "Option 1\nOption 2\nOption 3\nOption 4");
            lv_dropdown_set_selected(dropdown, 0);
            lv_obj_add_event_cb(dropdown, dropdown_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
        }

        /* Roller 标签 */
        lv_obj_t * roller_title = lv_label_create(container);
        if(roller_title != NULL)
        {
            lv_label_set_text(roller_title, "Roller:");
            lv_obj_set_style_text_font(roller_title, &lv_font_montserrat_14, 0);
            lv_obj_set_width(roller_title, 200);
        }

        /* Roller 控件 */
        lv_obj_t * roller = lv_roller_create(container);
        if(roller != NULL)
        {
            lv_obj_set_width(roller, 200);
            lv_roller_set_options(roller, "1\n2\n3\n4\n5", LV_ROLLER_MODE_NORMAL);
            lv_roller_set_selected(roller, 0, LV_ANIM_OFF);
            lv_obj_add_event_cb(roller, roller_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
        }
    }

    return scr;
}

/* Dropdown 值变化回调 */
static void dropdown_event_cb(lv_event_t * e)
{
    lv_obj_t * obj = lv_event_get_target(e);
    uint16_t selected = lv_dropdown_get_selected(obj);
    LV_LOG_USER("Dropdown selected: %d", selected);
}

/* Roller 值变化回调 */
static void roller_event_cb(lv_event_t * e)
{
    lv_obj_t * obj = lv_event_get_target(e);
    uint32_t selected = lv_roller_get_selected(obj);
    LV_LOG_USER("Roller selected: %lu", selected);
}

/* 返回按钮回调 */
static void back_btn_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    LV_LOG_USER("Navigating back to main menu");
    ui_navigate_to(g_main_menu_page);
}
