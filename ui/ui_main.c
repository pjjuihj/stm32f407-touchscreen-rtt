#include "ui.h"

/* 全局变量 */
lv_obj_t * g_current_page = NULL;
lv_obj_t * g_main_menu_page = NULL;

/* 页面创建函数声明 */
static void btn_input_cb(lv_event_t * e);
static void btn_display_cb(lv_event_t * e);
static void btn_data_cb(lv_event_t * e);
static void btn_selection_cb(lv_event_t * e);

/**
 * @brief 初始化 UI 界面
 */
void ui_init(void)
{
    /* 创建主菜单页面 */
    g_main_menu_page = ui_main_create();

    /* 加载主菜单页面 */
    lv_screen_load(g_main_menu_page);
    g_current_page = g_main_menu_page;
}

/**
 * @brief 初始化主菜单页面
 */
lv_obj_t * ui_main_create(void)
{
    /* 创建新屏幕 */
    lv_obj_t * scr = lv_obj_create(NULL);

    /* 设置屏幕背景色 */
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* 创建标题标签 */
    lv_obj_t * title = lv_label_create(scr);
    if(title != NULL)
    {
        lv_label_set_text(title, "LVGL 控件演示");
        lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    }

    /* 创建菜单列表 */
    lv_obj_t * list = lv_list_create(scr);
    if(list != NULL)
    {
        lv_obj_set_size(list, 200, 240);
        lv_obj_center(list);

        /* 输入控件按钮 */
        lv_obj_t * btn_input = lv_list_add_button(list, LV_SYMBOL_SETTINGS, "输入控件");
        if(btn_input != NULL)
        {
            lv_obj_add_event_cb(btn_input, btn_input_cb, LV_EVENT_CLICKED, NULL);
        }

        /* 显示控件按钮 */
        lv_obj_t * btn_display = lv_list_add_button(list, LV_SYMBOL_IMAGE, "显示控件");
        if(btn_display != NULL)
        {
            lv_obj_add_event_cb(btn_display, btn_display_cb, LV_EVENT_CLICKED, NULL);
        }

        /* 数据控件按钮 */
        lv_obj_t * btn_data = lv_list_add_button(list, LV_SYMBOL_LIST, "数据控件");
        if(btn_data != NULL)
        {
            lv_obj_add_event_cb(btn_data, btn_data_cb, LV_EVENT_CLICKED, NULL);
        }

        /* 选择控件按钮 */
        lv_obj_t * btn_selection = lv_list_add_button(list, LV_SYMBOL_OK, "选择控件");
        if(btn_selection != NULL)
        {
            lv_obj_add_event_cb(btn_selection, btn_selection_cb, LV_EVENT_CLICKED, NULL);
        }
    }

    return scr;
}

/* 导航到输入控件页面 */
static void btn_input_cb(lv_event_t * e)
{
    LV_LOG_USER("Navigating to input page");
    lv_obj_t * page = ui_input_create();
    ui_navigate_to(page);
}

/* 导航到显示控件页面 */
static void btn_display_cb(lv_event_t * e)
{
    LV_LOG_USER("Navigating to display page");
    lv_obj_t * page = ui_display_create();
    ui_navigate_to(page);
}

/* 导航到数据控件页面 */
static void btn_data_cb(lv_event_t * e)
{
    LV_LOG_USER("Navigating to data page");
    lv_obj_t * page = ui_data_create();
    ui_navigate_to(page);
}

/* 导航到选择控件页面 */
static void btn_selection_cb(lv_event_t * e)
{
    LV_LOG_USER("Navigating to selection page");
    lv_obj_t * page = ui_selection_create();
    ui_navigate_to(page);
}

/**
 * @brief 切换到指定页面
 */
void ui_navigate_to(lv_obj_t * page)
{
    if(page != NULL)
    {
        g_current_page = page;
        lv_screen_load_anim(page, LV_SCREEN_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
    }
}
