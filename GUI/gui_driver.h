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
 * @brief 写入自定义日志（供 Log_Write 调用）
 */
void gui_log_write(const char *str);

/**
 * @brief 日志输出模式
 */
typedef enum {
    LOG_OUTPUT_RTT = 0,     // RTT 输出
    LOG_OUTPUT_UART = 1,    // UART 输出
} log_output_mode_t;

/**
 * @brief 设置日志输出模式
 * @param mode: LOG_OUTPUT_RTT 或 LOG_OUTPUT_UART
 */
void gui_log_set_output_mode(log_output_mode_t mode);

/**
 * @brief 获取当前日志输出模式
 * @return 当前模式
 */
log_output_mode_t gui_log_get_output_mode(void);

#endif /* __GUI_DRIVER_H */
