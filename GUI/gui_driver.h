/**
 * @file gui_driver.h
 * @brief LVGL驱动适配层接口
 */

#ifndef __GUI_DRIVER_H
#define __GUI_DRIVER_H

#include "lvgl.h"

/**
 * @brief 初始化显示驱动
 */
void gui_disp_init(void);

/**
 * @brief 初始化触摸驱动
 */
void gui_touch_init(void);

/**
 * @brief 初始化时钟驱动
 */
void gui_tick_init(void);

/**
 * @brief 初始化日志功能
 */
void gui_log_init(void);

#endif /* __GUI_DRIVER_H */
