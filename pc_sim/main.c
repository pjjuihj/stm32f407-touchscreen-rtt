/**
 * @file main.c
 * @brief LVGL PC 模拟主程序
 *
 * 使用 SDL2 作为显示后端，在 PC 上运行 LVGL UI。
 */

#define SDL_MAIN_HANDLED  /* 修复 SDL 的 "undefined reference to WinMain" 问题 */
#include <SDL2/SDL.h>

#include "lvgl.h"
#include "sdl_driver.h"
#include "ui.h"

int main(void)
{
    /* 初始化 LVGL */
    lv_init();
    LV_LOG_USER("LVGL initialized");

    /* 初始化 SDL2 驱动 */
    sdl_driver_init();

    /* 初始化 UI（共享代码） */
    ui_init();
    LV_LOG_USER("UI initialized");

    /* 强制刷新一次屏幕 */
    lv_refr_now(NULL);

    /* 主循环 */
    LV_LOG_USER("Starting main loop...");
    while (1) {
        lv_timer_handler();
        SDL_Delay(5);  /* 5ms，匹配 STM32 的 delay_ms(5) */
    }

    return 0;
}
