/**
 * @file gui_fonts.h
 * @brief LVGL字体管理接口
 */

#ifndef __GUI_FONTS_H
#define __GUI_FONTS_H

#include "lvgl.h"

/**
 * @brief 初始化字体系统
 */
void gui_fonts_init(void);

/**
 * @brief 获取中文字体
 * @param size 字体大小 (16, 24, 32)
 * @return 字体指针
 */
const lv_font_t* gui_fonts_get_cn(uint8_t size);

/**
 * @brief 获取英文字体
 * @param size 字体大小 (14, 16, 18, 24)
 * @return 字体指针
 */
const lv_font_t* gui_fonts_get_en(uint8_t size);

#endif /* __GUI_FONTS_H */
