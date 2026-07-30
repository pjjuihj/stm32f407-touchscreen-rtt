#ifndef UI_SWIPE_H
#define UI_SWIPE_H

#include "lvgl.h"

/* 页面索引定义 */
#define SWIPE_PAGE_INPUT      0
#define SWIPE_PAGE_DISPLAY    1
#define SWIPE_PAGE_DATA       2
#define SWIPE_PAGE_SELECTION  3
#define SWIPE_PAGE_COUNT      4

/**
 * @brief 创建并初始化滑动导航
 *
 * 创建 lv_tileview，添加 4 个 tile，每个 tile 包含对应页面的内容。
 */
void ui_swipe_init(void);

/**
 * @brief 切换到指定页面
 * @param page_index 目标页面索引 (0-3)
 */
void ui_swipe_goto(uint8_t page_index);

/**
 * @brief 获取当前页面索引
 * @return 当前页面索引 (0-3)
 */
uint8_t ui_swipe_get_current(void);

/**
 * @brief 切换到主菜单页面
 */
void ui_swipe_goto_main(void);

#endif /* UI_SWIPE_H */
