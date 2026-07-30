#include "ui.h"
#include "ui_swipe.h"

/* 静态变量 */
static lv_obj_t * tileview = NULL;
static lv_obj_t * tiles[SWIPE_PAGE_COUNT] = {NULL};
static uint8_t current_page = SWIPE_PAGE_INPUT;

/* 页面创建函数声明已包含在 ui.h 中 */

/**
 * @brief tileview VALUE_CHANGED 回调
 *
 * 当用户通过触摸滑动切换 tile 时，LVGL 会触发此事件。
 * 用于同步 current_page 变量，使其与实际显示的 tile 一致。
 */
static void tileview_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code != LV_EVENT_VALUE_CHANGED)
    {
        return;
    }

    lv_obj_t * active_tile = lv_tileview_get_tile_act(tileview);
    if(active_tile == NULL)
    {
        return;
    }

    for(uint8_t i = 0; i < SWIPE_PAGE_COUNT; i++)
    {
        if(tiles[i] == active_tile)
        {
            if(current_page != i)
            {
                current_page = i;
                LV_LOG_USER("Tile changed to page %d (swipe)", i);
            }
            break;
        }
    }
}

/**
 * @brief tileview GESTURE 回调（循环导航）
 *
 * 在 Main Menu 和 Selection 两端处理循环滑动：
 * - Main Menu 左滑 -> 跳转到 Selection
 * - Selection 右滑 -> 跳转到 Main Menu
 */
static void tileview_gesture_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code != LV_EVENT_GESTURE)
    {
        return;
    }

    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());

    if(current_page == SWIPE_PAGE_MAIN && dir == LV_DIR_LEFT)
    {
        /* Main Menu 左滑 -> 循环到 Selection */
        lv_obj_set_tile(tileview, tiles[SWIPE_PAGE_SELECTION], LV_ANIM_ON);
        current_page = SWIPE_PAGE_SELECTION;
        LV_LOG_USER("Circular nav: Main -> Selection");
    }
    else if(current_page == SWIPE_PAGE_SELECTION && dir == LV_DIR_RIGHT)
    {
        /* Selection 右滑 -> 循环到 Main Menu */
        lv_obj_set_tile(tileview, tiles[SWIPE_PAGE_MAIN], LV_ANIM_ON);
        current_page = SWIPE_PAGE_MAIN;
        LV_LOG_USER("Circular nav: Selection -> Main");
    }
}

/**
 * @brief 创建并初始化滑动导航
 */
void ui_swipe_init(void)
{
    /* 创建 tileview */
    tileview = lv_tileview_create(lv_screen_active());
    if(tileview == NULL)
    {
        LV_LOG_ERROR("Failed to create tileview");
        return;
    }

    /* 设置 tileview 全屏 */
    lv_obj_set_size(tileview, LV_PCT(100), LV_PCT(100));

    /* 注册 VALUE_CHANGED 回调，用于同步用户手动滑动时的 current_page */
    lv_obj_add_event_cb(tileview, tileview_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* 注册 GESTURE 回调，用于处理循环导航 */
    lv_obj_add_event_cb(tileview, tileview_gesture_cb, LV_EVENT_GESTURE, NULL);

    /* 添加 Main Menu tile (0, 0) - 支持左右滑（左滑由 gesture_cb 处理循环导航） */
    tiles[SWIPE_PAGE_MAIN] = lv_tileview_add_tile(tileview, 0, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    if(tiles[SWIPE_PAGE_MAIN] != NULL)
    {
        ui_main_create(tiles[SWIPE_PAGE_MAIN]);
    }

    /* 添加 Input tile (1, 0) - 支持左右滑 */
    tiles[SWIPE_PAGE_INPUT] = lv_tileview_add_tile(tileview, 1, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    if(tiles[SWIPE_PAGE_INPUT] != NULL)
    {
        ui_input_create(tiles[SWIPE_PAGE_INPUT]);
    }

    /* 添加 Display tile (2, 0) - 支持左右滑 */
    tiles[SWIPE_PAGE_DISPLAY] = lv_tileview_add_tile(tileview, 2, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    if(tiles[SWIPE_PAGE_DISPLAY] != NULL)
    {
        ui_display_create(tiles[SWIPE_PAGE_DISPLAY]);
    }

    /* 添加 Data tile (3, 0) - 支持左右滑 */
    tiles[SWIPE_PAGE_DATA] = lv_tileview_add_tile(tileview, 3, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    if(tiles[SWIPE_PAGE_DATA] != NULL)
    {
        ui_data_create(tiles[SWIPE_PAGE_DATA]);
    }

    /* 添加 Selection tile (4, 0) - 支持左右滑（右滑由 gesture_cb 处理循环导航） */
    tiles[SWIPE_PAGE_SELECTION] = lv_tileview_add_tile(tileview, 4, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    if(tiles[SWIPE_PAGE_SELECTION] != NULL)
    {
        ui_selection_create(tiles[SWIPE_PAGE_SELECTION]);
    }

    /* 设置初始页面为主菜单 */
    lv_obj_set_tile(tileview, tiles[SWIPE_PAGE_MAIN], LV_ANIM_OFF);
    current_page = SWIPE_PAGE_MAIN;

    LV_LOG_USER("Swipe navigation initialized");
}

/**
 * @brief 切换到指定页面
 */
void ui_swipe_goto(uint8_t page_index)
{
    if(page_index < SWIPE_PAGE_COUNT && tiles[page_index] != NULL)
    {
        lv_obj_set_tile(tileview, tiles[page_index], LV_ANIM_ON);
        current_page = page_index;
        LV_LOG_USER("Navigated to page %d", page_index);
    }
}

/**
 * @brief 获取当前页面索引
 */
uint8_t ui_swipe_get_current(void)
{
    return current_page;
}

/**
 * @brief 切换到主菜单页面
 */
void ui_swipe_goto_main(void)
{
    LV_LOG_USER("Navigating to main menu");
    ui_swipe_goto(SWIPE_PAGE_MAIN);
}
