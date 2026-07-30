#include "ui.h"
#include "ui_swipe.h"

/* 静态变量 */
static lv_obj_t * tileview = NULL;
static lv_obj_t * tiles[SWIPE_PAGE_COUNT] = {NULL};
static uint8_t current_page = SWIPE_PAGE_INPUT;

/* 页面创建函数声明 (来自 ui.h，无参数版本) */
extern lv_obj_t * ui_input_create(void);
extern lv_obj_t * ui_display_create(void);
extern lv_obj_t * ui_data_create(void);
extern lv_obj_t * ui_selection_create(void);

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
 * @brief 将临时屏幕的子对象移动到 tile 中
 *
 * ui_*_create() 会创建独立的屏幕（lv_obj_create(NULL)），
 * 此函数将屏幕上的所有子控件移动到 tile 中，然后删除临时屏幕。
 *
 * @param tile 目标 tile 对象
 * @param temp_screen 临时屏幕对象
 */
static void move_children_to_tile(lv_obj_t * tile, lv_obj_t * temp_screen)
{
    if(tile == NULL || temp_screen == NULL)
    {
        return;
    }

    uint32_t count = lv_obj_get_child_count(temp_screen);
    for(uint32_t i = 0; i < count; i++)
    {
        lv_obj_t * child = lv_obj_get_child(temp_screen, 0);
        if(child == NULL)
        {
            break;
        }
        lv_obj_set_parent(child, tile);
    }

    lv_obj_del(temp_screen);
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

    /* 添加 Input tile (0, 0) - 支持左滑 */
    tiles[SWIPE_PAGE_INPUT] = lv_tileview_add_tile(tileview, 0, 0, LV_DIR_LEFT);
    if(tiles[SWIPE_PAGE_INPUT] != NULL)
    {
        lv_obj_t * scr = ui_input_create();
        move_children_to_tile(tiles[SWIPE_PAGE_INPUT], scr);
    }

    /* 添加 Display tile (1, 0) - 支持左右滑 */
    tiles[SWIPE_PAGE_DISPLAY] = lv_tileview_add_tile(tileview, 1, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    if(tiles[SWIPE_PAGE_DISPLAY] != NULL)
    {
        lv_obj_t * scr = ui_display_create();
        move_children_to_tile(tiles[SWIPE_PAGE_DISPLAY], scr);
    }

    /* 添加 Data tile (2, 0) - 支持左右滑 */
    tiles[SWIPE_PAGE_DATA] = lv_tileview_add_tile(tileview, 2, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    if(tiles[SWIPE_PAGE_DATA] != NULL)
    {
        lv_obj_t * scr = ui_data_create();
        move_children_to_tile(tiles[SWIPE_PAGE_DATA], scr);
    }

    /* 添加 Selection tile (3, 0) - 支持右滑 */
    tiles[SWIPE_PAGE_SELECTION] = lv_tileview_add_tile(tileview, 3, 0, LV_DIR_RIGHT);
    if(tiles[SWIPE_PAGE_SELECTION] != NULL)
    {
        lv_obj_t * scr = ui_selection_create();
        move_children_to_tile(tiles[SWIPE_PAGE_SELECTION], scr);
    }

    /* 设置初始页面 */
    lv_obj_set_tile(tileview, tiles[SWIPE_PAGE_INPUT], LV_ANIM_OFF);
    current_page = SWIPE_PAGE_INPUT;

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
    if(g_main_menu_page == NULL)
    {
        LV_LOG_ERROR("g_main_menu_page is not initialized, cannot navigate to main menu");
        return;
    }
    LV_LOG_USER("Navigating to main menu");
    lv_screen_load(g_main_menu_page);
}
