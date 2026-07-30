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

/**
 * @brief 主循环中调用 - 发送缓冲区中的日志
 */
void gui_log_flush(void);

/**
 * @brief 获取显示刷新计数
 */
uint32_t gui_get_flush_count(void);

#endif /* __GUI_DRIVER_H */
