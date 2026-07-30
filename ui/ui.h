#ifndef UI_H
#define UI_H

#include "lvgl.h"

/* 页面索引 */
#define PAGE_MAIN       0
#define PAGE_INPUT      1
#define PAGE_DISPLAY    2
#define PAGE_DATA       3
#define PAGE_SELECTION  4
#define PAGE_COUNT      5

/* 全局变量：当前页面 */
extern lv_obj_t * g_current_page;
extern lv_obj_t * g_main_menu_page;

/**
 * @brief 初始化 UI 界面
 *
 * 创建所有 UI 控件，设置事件回调。
 * 此函数是平台无关的，PC 和 STM32 共享。
 */
void ui_init(void);

/**
 * @brief 初始化主菜单页面
 * @return 主菜单页面对象
 */
lv_obj_t * ui_main_create(void);

/**
 * @brief 初始化输入控件页面
 * @param parent 父对象（tileview tile）
 * @return 输入控件页面对象
 */
lv_obj_t * ui_input_create(lv_obj_t * parent);

/**
 * @brief 初始化显示控件页面
 * @param parent 父对象（tileview tile）
 * @return 显示控件页面对象
 */
lv_obj_t * ui_display_create(lv_obj_t * parent);

/**
 * @brief 初始化数据控件页面
 * @param parent 父对象（tileview tile）
 * @return 数据控件页面对象
 */
lv_obj_t * ui_data_create(lv_obj_t * parent);

/**
 * @brief 初始化选择控件页面
 * @param parent 父对象（tileview tile）
 * @return 选择控件页面对象
 */
lv_obj_t * ui_selection_create(lv_obj_t * parent);

/**
 * @brief 切换到指定页面
 * @param page 目标页面对象
 */
void ui_navigate_to(lv_obj_t * page);

#endif /* UI_H */
