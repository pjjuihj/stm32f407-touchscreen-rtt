#include "ui_bottom_nav.h"

/* 静态变量 */
static lv_obj_t * nav_buttons[BOTTOM_NAV_COUNT] = {NULL};
static uint8_t active_index = BOTTOM_NAV_HOME;

/* 回调函数 */
static void back_btn_cb(lv_event_t * e);
static void home_btn_cb(lv_event_t * e);
static void recent_btn_cb(lv_event_t * e);
static void notify_btn_cb(lv_event_t * e);

/* 按钮回调数组 */
static void (*nav_callbacks[BOTTOM_NAV_COUNT])(lv_event_t * e) = {
    back_btn_cb, home_btn_cb, recent_btn_cb, notify_btn_cb
};

/**
 * @brief 创建底部导航栏组件
 */
lv_obj_t * ui_bottom_nav_create(lv_obj_t * parent)
{
    /* 创建导航栏容器 */
    lv_obj_t * nav_bar = lv_obj_create(parent);
    if(nav_bar == NULL) return NULL;

    /* 隐藏滚动条 */
    lv_obj_set_scrollbar_mode(nav_bar, LV_SCROLLBAR_MODE_OFF);

    /* 设置导航栏大小和样式 */
    lv_obj_set_size(nav_bar, LV_PCT(100), 50);
    lv_obj_set_style_bg_color(nav_bar, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(nav_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(nav_bar, 0, 0);
    lv_obj_set_style_pad_all(nav_bar, 0, 0);

    /* 设置 Flex 布局 */
    lv_obj_set_flex_flow(nav_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(nav_bar, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* 创建导航按钮 */
    const char * symbols[BOTTOM_NAV_COUNT] = {
        LV_SYMBOL_LEFT, LV_SYMBOL_HOME, LV_SYMBOL_IMAGE, LV_SYMBOL_BELL
    };

    for(uint8_t i = 0; i < BOTTOM_NAV_COUNT; i++)
    {
        nav_buttons[i] = lv_button_create(nav_bar);
        if(nav_buttons[i] != NULL)
        {
            lv_obj_set_size(nav_buttons[i], 50, 40);
            lv_obj_set_style_bg_opa(nav_buttons[i], LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(nav_buttons[i], 0, 0);

            lv_obj_t * label = lv_label_create(nav_buttons[i]);
            if(label != NULL)
            {
                lv_label_set_text(label, symbols[i]);
                lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
                lv_obj_set_style_text_color(label, lv_color_hex(0x757575), 0);
                lv_obj_center(label);
            }

            lv_obj_add_event_cb(nav_buttons[i], nav_callbacks[i], LV_EVENT_CLICKED, NULL);
        }
    }

    /* 设置默认活跃按钮 */
    ui_bottom_nav_set_active(BOTTOM_NAV_HOME);

    return nav_bar;
}

/**
 * @brief 设置当前活跃的导航按钮
 */
void ui_bottom_nav_set_active(uint8_t index)
{
    if(index >= BOTTOM_NAV_COUNT) return;

    /* 取消所有按钮的高亮状态 */
    for(uint8_t i = 0; i < BOTTOM_NAV_COUNT; i++)
    {
        if(nav_buttons[i] != NULL)
        {
            lv_obj_t * label = lv_obj_get_child(nav_buttons[i], 0);
            if(label != NULL)
            {
                lv_obj_set_style_text_color(label, lv_color_hex(0x757575), 0);
            }
        }
    }

    /* 高亮当前按钮 */
    if(nav_buttons[index] != NULL)
    {
        lv_obj_t * label = lv_obj_get_child(nav_buttons[index], 0);
        if(label != NULL)
        {
            lv_obj_set_style_text_color(label, lv_color_hex(0x2196F3), 0);
        }
    }

    active_index = index;
}

/* 返回按钮回调 */
static void back_btn_cb(lv_event_t * e)
{
    LV_LOG_USER("Back button clicked");
    /* TODO: 实现返回功能 */
}

/* 主页按钮回调 */
static void home_btn_cb(lv_event_t * e)
{
    LV_LOG_USER("Home button clicked");
    /* TODO: 实现主页功能 */
}

/* 最近任务按钮回调 */
static void recent_btn_cb(lv_event_t * e)
{
    LV_LOG_USER("Recent button clicked");
    /* TODO: 实现最近任务功能 */
}

/* 通知按钮回调 */
static void notify_btn_cb(lv_event_t * e)
{
    LV_LOG_USER("Notify button clicked");
    /* TODO: 实现通知功能 */
}
