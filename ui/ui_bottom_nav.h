#ifndef UI_BOTTOM_NAV_H
#define UI_BOTTOM_NAV_H

#include "lvgl.h"

/* 导航按钮索引 */
#define BOTTOM_NAV_BACK     0
#define BOTTOM_NAV_HOME     1
#define BOTTOM_NAV_RECENT   2
#define BOTTOM_NAV_NOTIFY   3
#define BOTTOM_NAV_COUNT    4

/**
 * @brief 创建底部导航栏组件
 * @param parent 父对象
 * @return 底部导航栏对象
 */
lv_obj_t * ui_bottom_nav_create(lv_obj_t * parent);

/**
 * @brief 设置当前活跃的导航按钮
 * @param index 按钮索引
 */
void ui_bottom_nav_set_active(uint8_t index);

#endif /* UI_BOTTOM_NAV_H */
