#include "ui.h"
#include "ui_swipe.h"

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
    /* 初始化滑动导航（创建 tileview，主菜单作为第一个 tile） */
    ui_swipe_init();

    /* 主菜单已作为 tileview 的第一个 tile 创建，无需单独加载 */
    g_current_page = g_main_menu_page;
}

/**
 * @brief 初始化主菜单页面
 * @param parent 父对象（tileview tile）
 */
lv_obj_t * ui_main_create(lv_obj_t * parent)
{
    /* 创建容器（作为 tileview tile 的子对象） */
    lv_obj_t * scr = lv_obj_create(parent);

    /* 设置容器大小为 100% 以填满 tile */
    lv_obj_set_size(scr, LV_PCT(100), LV_PCT(100));

    /* 记录主菜单页面引用（用于返回导航） */
    g_main_menu_page = scr;

    /* 设置容器背景色 */
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* 创建标题标签 */
    lv_obj_t * title = lv_label_create(scr);
    if(title != NULL)
    {
        lv_label_set_text(title, "LVGL Widget Demo");
        lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    }

    /* 创建菜单列表 */
    lv_obj_t * list = lv_list_create(scr);
    if(list != NULL)
    {
        lv_obj_set_size(list, LV_PCT(100), LV_PCT(100));
        lv_obj_set_style_pad_all(list, 10, 0);

        /* 输入控件按钮 */
        lv_obj_t * btn_input = lv_list_add_button(list, LV_SYMBOL_SETTINGS, "Input");
        if(btn_input != NULL)
        {
            lv_obj_add_event_cb(btn_input, btn_input_cb, LV_EVENT_CLICKED, NULL);
        }

        /* 显示控件按钮 */
        lv_obj_t * btn_display = lv_list_add_button(list, LV_SYMBOL_IMAGE, "Display");
        if(btn_display != NULL)
        {
            lv_obj_add_event_cb(btn_display, btn_display_cb, LV_EVENT_CLICKED, NULL);
        }

        /* 数据控件按钮 */
        lv_obj_t * btn_data = lv_list_add_button(list, LV_SYMBOL_LIST, "Data");
        if(btn_data != NULL)
        {
            lv_obj_add_event_cb(btn_data, btn_data_cb, LV_EVENT_CLICKED, NULL);
        }

        /* 选择控件按钮 */
        lv_obj_t * btn_selection = lv_list_add_button(list, LV_SYMBOL_OK, "Selection");
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
    ui_swipe_goto(SWIPE_PAGE_INPUT);
}

/* 导航到显示控件页面 */
static void btn_display_cb(lv_event_t * e)
{
    LV_LOG_USER("Navigating to display page");
    ui_swipe_goto(SWIPE_PAGE_DISPLAY);
}

/* 导航到数据控件页面 */
static void btn_data_cb(lv_event_t * e)
{
    LV_LOG_USER("Navigating to data page");
    ui_swipe_goto(SWIPE_PAGE_DATA);
}

/* 导航到选择控件页面 */
static void btn_selection_cb(lv_event_t * e)
{
    LV_LOG_USER("Navigating to selection page");
    ui_swipe_goto(SWIPE_PAGE_SELECTION);
}
