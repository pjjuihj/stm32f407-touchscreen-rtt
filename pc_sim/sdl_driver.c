#include "lvgl.h"
#include "sdl_driver.h"

#define SDL_MAIN_HANDLED  /* 修复 SDL 的 "undefined reference to WinMain" 问题 */
#include <SDL2/SDL.h>

/* LVGL SDL 驱动头文件 */
#include "src/drivers/sdl/lv_sdl_window.h"
#include "src/drivers/sdl/lv_sdl_mouse.h"
#include "src/drivers/sdl/lv_sdl_mousewheel.h"
#include "src/drivers/sdl/lv_sdl_keyboard.h"

/* 静态变量 */
static lv_display_t * disp = NULL;
static lv_indev_t * mouse = NULL;
static lv_indev_t * mousewheel = NULL;
static lv_indev_t * keyboard = NULL;

void sdl_driver_init(void)
{
    /* 初始化 SDL 视频子系统 */
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        LV_LOG_ERROR("SDL_Init failed: %s", SDL_GetError());
        return;
    }

    /* 创建 LVGL 显示窗口（匹配硬件分辨率 240x320） */
    disp = lv_sdl_window_create(240, 320);
    if (disp == NULL) {
        LV_LOG_ERROR("lv_sdl_window_create failed");
        return;
    }
    lv_sdl_window_set_title(disp, "LVGL PC Sim");

    /* 创建鼠标输入设备 */
    mouse = lv_sdl_mouse_create();
    if (mouse == NULL) {
        LV_LOG_ERROR("lv_sdl_mouse_create failed");
    }

    /* 创建鼠标滚轮输入设备 */
    mousewheel = lv_sdl_mousewheel_create();
    if (mousewheel == NULL) {
        LV_LOG_ERROR("lv_sdl_mousewheel_create failed");
    }

    /* 创建键盘输入设备 */
    keyboard = lv_sdl_keyboard_create();
    if (keyboard == NULL) {
        LV_LOG_ERROR("lv_sdl_keyboard_create failed");
    }

    LV_LOG_USER("SDL2 driver initialized");
}
