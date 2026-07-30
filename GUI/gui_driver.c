/**
 * @file gui_driver.c
 * @brief LVGL驱动适配层实现
 */

#include "gui_driver.h"
#include "lcd.h"
#include "touch.h"
#include "xpt2046.h"
#include "usart.h"
#include "stm32f4xx_hal.h"
#include <stdio.h>

/*===========================================================================
 * 显示驱动配置
 *===========================================================================*/

/* LCD分辨率 */
#define MY_DISP_HOR_RES  240
#define MY_DISP_VER_RES  320

/* 缓冲区行数 */
#define BUF_LINES        10

/* 双缓冲区 */
static lv_color_t buf1[MY_DISP_HOR_RES * BUF_LINES];
static lv_color_t buf2[MY_DISP_HOR_RES * BUF_LINES];

/**
 * @brief LVGL刷新回调函数
 */
static void disp_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    uint32_t width = area->x2 - area->x1 + 1;
    uint32_t height = area->y2 - area->y1 + 1;
    uint32_t size = width * height;

    /* 设置显示窗口 */
    LCD_Open_Window(area->x1, area->y1, width, height);
    LCD_WriteGRAM();

    /* 发送颜色数据 - 简化版本 */
    uint16_t *color_p16 = (uint16_t *)px_map;
    uint32_t i;
    for(i = 0; i < size; i++) {
        LCD_DATA = color_p16[i];
    }

    /* 通知LVGL刷新完成 */
    lv_display_flush_ready(disp);
}

/**
 * @brief 初始化显示驱动
 */
void gui_disp_init(void)
{
    /* 创建显示设备 */
    lv_display_t *disp = lv_display_create(MY_DISP_HOR_RES, MY_DISP_VER_RES);

    if(disp == NULL) {
        return;
    }

    /* 设置刷新回调 */
    lv_display_set_flush_cb(disp, disp_flush_cb);

    /* 设置缓冲区 */
    lv_display_set_buffers(disp, buf1, buf2, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
}

/*===========================================================================
 * 触摸驱动配置
 *===========================================================================*/

/**
 * @brief 触摸读取回调
 */
static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    static lv_coord_t last_x = 0, last_y = 0;

    /* 扫描触摸 */
    XPT2046_Scan(0);

    /* 检查触摸状态 */
    if(Xdown != 0xffff && Ydown != 0xffff) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = Xdown;
        data->point.y = Ydown;
        last_x = data->point.x;
        last_y = data->point.y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
        data->point.x = last_x;
        data->point.y = last_y;
    }
}

/**
 * @brief 初始化触摸驱动
 */
void gui_touch_init(void)
{
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read_cb);

    /* 设置触摸关联的显示设备 */
    lv_display_t *disp = lv_display_get_default();
    if(disp != NULL) {
        lv_indev_set_display(indev, disp);
    }
}

/*===========================================================================
 * 时钟驱动配置
 *===========================================================================*/

/**
 * @brief 官方时钟回调函数
 */
static uint32_t gui_tick_get(void)
{
    return HAL_GetTick();
}

/**
 * @brief 初始化时钟驱动
 */
void gui_tick_init(void)
{
    lv_tick_set_cb(gui_tick_get);
}

/*===========================================================================
 * 日志功能配置 - 环形缓冲区方式（不阻塞）
 *===========================================================================*/

#if LV_USE_LOG
#define LV_LOG_BUF_SIZE 1024
static char lv_log_buf[LV_LOG_BUF_SIZE];
static volatile uint16_t lv_log_head = 0;
static volatile uint16_t lv_log_tail = 0;

static void lv_log_put_char(char c)
{
    uint16_t next = (lv_log_head + 1) % LV_LOG_BUF_SIZE;
    if(next != lv_log_tail) {
        lv_log_buf[lv_log_head] = c;
        lv_log_head = next;
    }
}

/**
 * @brief LVGL日志回调 - 仅写入缓冲区，不访问UART（安全）
 */
static void lv_log_print_g_cb(lv_log_level_t level, const char *buf)
{
    const char *prefix;
    switch(level) {
        case LV_LOG_LEVEL_TRACE: prefix = "[TRACE] "; break;
        case LV_LOG_LEVEL_INFO:  prefix = "[INFO]  "; break;
        case LV_LOG_LEVEL_WARN:  prefix = "[WARN]  "; break;
        case LV_LOG_LEVEL_ERROR: prefix = "[ERROR] "; break;
        case LV_LOG_LEVEL_USER:  prefix = "[USER]  "; break;
        default:                 prefix = "[???]   "; break;
    }
    for(const char *p = prefix; *p; p++) lv_log_put_char(*p);
    for(const char *p = buf; *p; p++) lv_log_put_char(*p);
    lv_log_put_char('\r');
    lv_log_put_char('\n');
}
#endif

/**
 * @brief 初始化日志功能（必须在lv_init之前调用）
 */
void gui_log_init(void)
{
#if LV_USE_LOG
    lv_log_head = 0;
    lv_log_tail = 0;
    lv_log_register_print_cb(lv_log_print_g_cb);
#endif
}

/**
 * @brief 主循环调用 - 从缓冲区发送到UART
 */
void gui_log_flush(void)
{
#if LV_USE_LOG
    while(lv_log_tail != lv_log_head) {
        USART1_SendChar(lv_log_buf[lv_log_tail]);
        lv_log_tail = (lv_log_tail + 1) % LV_LOG_BUF_SIZE;
    }
#endif
}
