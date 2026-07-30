#include "ui.h"
#include "ui_swipe.h"
#include "ui_status_bar.h"
#include "ui_bottom_nav.h"
#include "ui_notification.h"

/* 全局变量 */
lv_obj_t * g_current_page = NULL;
lv_obj_t * g_main_menu_page = NULL;

/* 应用图标数据 */
typedef struct {
    const char * name;
    const char * symbol;
    uint8_t page_index;
} app_info_t;

static const app_info_t apps[] = {
    {"Input", LV_SYMBOL_SETTINGS, SWIPE_PAGE_INPUT},
    {"Display", LV_SYMBOL_IMAGE, SWIPE_PAGE_DISPLAY},
    {"Data", LV_SYMBOL_LIST, SWIPE_PAGE_DATA},
    {"Selection", LV_SYMBOL_OK, SWIPE_PAGE_SELECTION},
};

#define APP_COUNT (sizeof(apps) / sizeof(apps[0]))

/* 回调函数 */
static void app_icon_cb(lv_event_t * e);

/**
 * @brief 初始化 UI 界面
 */
void ui_init(void)
{
    /* 初始化滑动导航 */
    ui_swipe_init();

    /* 主菜单已作为 tileview 的第一个 tile 创建 */
    g_current_page = g_main_menu_page;
}

/**
 * @brief 初始化主屏幕页面
 * @param parent 父对象（tileview tile）
 */
lv_obj_t * ui_main_create(lv_obj_t * parent)
{
    /* 创建容器 */
    lv_obj_t * scr = lv_obj_create(parent);
    if(scr == NULL) return NULL;

    /* 设置容器大小 */
    lv_obj_set_size(scr, LV_PCT(100), LV_PCT(100));

    /* 隐藏滚动条 */
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);

    /* 记录主菜单页面引用 */
    g_main_menu_page = scr;

    /* 设置容器背景色 */
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* 创建状态栏 */
    ui_status_bar_create(scr);

    /* 创建应用图标网格 */
    lv_obj_t * app_grid = lv_obj_create(scr);
    if(app_grid != NULL)
    {
        lv_obj_set_size(app_grid, LV_PCT(100), LV_PCT(100));
        lv_obj_set_scrollbar_mode(app_grid, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_scroll_dir(app_grid, LV_DIR_NONE);
        lv_obj_align(app_grid, LV_ALIGN_TOP_MID, 0, 30);
        lv_obj_set_style_bg_opa(app_grid, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(app_grid, 0, 0);
        lv_obj_set_style_pad_all(app_grid, 10, 0);

        /* 设置 4x3 网格 */
        lv_obj_set_flex_flow(app_grid, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_flex_align(app_grid, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_row(app_grid, 20, 0);
        lv_obj_set_style_pad_column(app_grid, 20, 0);

        /* 创建应用图标 */
        for(uint8_t i = 0; i < APP_COUNT; i++)
        {
            lv_obj_t * app_icon = lv_obj_create(app_grid);
            if(app_icon != NULL)
            {
                lv_obj_set_size(app_icon, 60, 70);
                lv_obj_set_style_bg_color(app_icon, lv_color_hex(0xE3F2FD), 0);
                lv_obj_set_style_bg_opa(app_icon, LV_OPA_COVER, 0);
                lv_obj_set_style_radius(app_icon, 15, 0);
                lv_obj_set_style_border_width(app_icon, 0, 0);
                lv_obj_set_style_shadow_width(app_icon, 10, 0);
                lv_obj_set_style_shadow_color(app_icon, lv_color_hex(0x000000), 0);
                lv_obj_set_style_shadow_opa(app_icon, LV_OPA_20, 0);
                lv_obj_set_style_outline_width(app_icon, 0, 0);
                lv_obj_set_style_outline_opa(app_icon, LV_OPA_TRANSP, 0);

                /* 创建图标 */
                lv_obj_t * icon = lv_label_create(app_icon);
                if(icon != NULL)
                {
                    lv_label_set_text(icon, apps[i].symbol);
                    lv_obj_set_style_text_font(icon, &lv_font_montserrat_16, 0);
                    lv_obj_set_style_text_color(icon, lv_color_hex(0x2196F3), 0);
                    lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 5);
                }

                /* 创建名称标签 */
                lv_obj_t * name = lv_label_create(app_icon);
                if(name != NULL)
                {
                    lv_label_set_text(name, apps[i].name);
                    lv_obj_set_style_text_font(name, &lv_font_montserrat_12, 0);
                    lv_obj_set_style_text_color(name, lv_color_hex(0x333333), 0);
                    lv_obj_align(name, LV_ALIGN_BOTTOM_MID, 0, 0);
                }

                /* 添加点击事件 */
                lv_obj_add_event_cb(app_icon, app_icon_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
            }
        }
    }

    /* 创建底部导航栏 */
    ui_bottom_nav_create(scr);

    /* 创建通知中心 */
    ui_notification_create(scr);

    return scr;
}

/* 应用图标点击回调 */
static void app_icon_cb(lv_event_t * e)
{
    uint8_t index = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if(index < APP_COUNT)
    {
        LV_LOG_USER("Launching app: %s", apps[index].name);
        ui_swipe_goto(apps[index].page_index);
    }
}
