#ifndef UI_H
#define UI_H

#include "lvgl.h"

/**
 * @brief 初始化 UI 界面
 *
 * 创建所有 UI 控件，设置事件回调。
 * 此函数是平台无关的，PC 和 STM32 共享。
 */
void ui_init(void);

#endif /* UI_H */
