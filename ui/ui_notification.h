#ifndef UI_NOTIFICATION_H
#define UI_NOTIFICATION_H

#include "lvgl.h"

/**
 * @brief 创建通知中心组件
 * @param parent 父对象
 * @return 通知中心对象
 */
lv_obj_t * ui_notification_create(lv_obj_t * parent);

/**
 * @brief 显示通知中心
 */
void ui_notification_show(void);

/**
 * @brief 隐藏通知中心
 */
void ui_notification_hide(void);

#endif /* UI_NOTIFICATION_H */
