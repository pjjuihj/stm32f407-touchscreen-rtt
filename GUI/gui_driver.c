#include "gui_driver.h"
#include "lcd.h"
#include "touch.h"
#include "xpt2046.h"
#include "usart.h"
#include "wifi.h"
#include "bluetooth.h"
#include "stm32f4xx_hal.h"
#include "SEGGER_RTT.h"

#define MY_DISP_HOR_RES 240
#define MY_DISP_VER_RES 320
#define BUF_LINES 10
#define RTT_CHANNEL_LVGL 0

static lv_color_t g_display_buffer_a[MY_DISP_HOR_RES * BUF_LINES];
static lv_color_t g_display_buffer_b[MY_DISP_HOR_RES * BUF_LINES];
static volatile log_output_mode_t g_log_output_mode = LOG_OUTPUT_UART;
static volatile gui_diag_snapshot_t g_gui_diag;

extern void USART1_ConsoleErrorCallback(UART_HandleTypeDef *uart);

static void DisplayFlushCallback(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    uint32_t width = (uint32_t)(area->x2 - area->x1 + 1);
    uint32_t height = (uint32_t)(area->y2 - area->y1 + 1);
    uint32_t pixel_count = width * height;
    uint16_t *pixel = (uint16_t *)pixels;
    uint32_t index;

    g_gui_diag.flush_started++;
    g_gui_diag.last_flush_x1 = (int16_t)area->x1;
    g_gui_diag.last_flush_y1 = (int16_t)area->y1;
    g_gui_diag.last_flush_x2 = (int16_t)area->x2;
    g_gui_diag.last_flush_y2 = (int16_t)area->y2;
    g_gui_diag.last_flush_pixels = pixel_count;

    LCD_Open_Window(area->x1, area->y1, (u16)width, (u16)height);
    LCD_WriteGRAM();
    for (index = 0U; index < pixel_count; index++) {
        LCD_DATA = pixel[index];
    }
    g_gui_diag.flush_completed++;
    g_gui_diag.last_flush_tick = HAL_GetTick();
    lv_display_flush_ready(display);
}

bool gui_disp_init(void)
{
    lv_display_t *display = lv_display_create(MY_DISP_HOR_RES, MY_DISP_VER_RES);
    if (display == NULL) {
        return false;
    }
    lv_display_set_flush_cb(display, DisplayFlushCallback);
    lv_display_set_buffers(display, g_display_buffer_a, g_display_buffer_b,
                           sizeof(g_display_buffer_a), LV_DISPLAY_RENDER_MODE_PARTIAL);
    return true;
}

static void TouchReadCallback(lv_indev_t *input, lv_indev_data_t *data)
{
    static lv_coord_t last_x;
    static lv_coord_t last_y;
    (void)input;

    g_gui_diag.touch_started++;
    XPT2046_Scan(0U);
    if (Xdown != 0xffffU && Ydown != 0xffffU) {
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
    g_gui_diag.touch_completed++;
    g_gui_diag.last_touch_tick = HAL_GetTick();
}

bool gui_touch_init(void)
{
    lv_display_t *display = lv_display_get_default();
    if (display == NULL) {
        return false;
    }
    lv_indev_t *input = lv_indev_create();
    if (input == NULL) {
        return false;
    }
    lv_indev_set_type(input, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(input, TouchReadCallback);
    lv_indev_set_display(input, display);
    return true;
}

static uint32_t GetLvglTick(void)
{
    return HAL_GetTick();
}

void gui_tick_init(void)
{
    lv_tick_set_cb(GetLvglTick);
}

void gui_log_set_output_mode(log_output_mode_t mode)
{
    g_log_output_mode = mode;
}

log_output_mode_t gui_log_get_output_mode(void)
{
    return g_log_output_mode;
}

static void LvglLogCallback(lv_log_level_t level, const char *message)
{
    (void)level;
    if (message == NULL) {
        return;
    }
    if (g_log_output_mode == LOG_OUTPUT_RTT) {
        SEGGER_RTT_WriteString(RTT_CHANNEL_LVGL, message);
        SEGGER_RTT_WriteString(RTT_CHANNEL_LVGL, "\r\n");
    } else {
        USART1_SendString(message);
        USART1_SendString("\r\n");
    }
}

void gui_log_init(void)
{
#if LV_USE_LOG
    lv_log_register_print_cb(LvglLogCallback);
#endif
}

void gui_log_flush(void)
{
    USART1_TxProcess();
}

void gui_log_write(const char *message)
{
    USART1_SendString(message);
}

void gui_diag_note_loop_completed(void)
{
    g_gui_diag.loop_completed++;
    g_gui_diag.last_loop_tick = HAL_GetTick();
}

void gui_diag_get_snapshot(gui_diag_snapshot_t *snapshot)
{
    if (snapshot == NULL) {
        return;
    }

    snapshot->loop_completed = g_gui_diag.loop_completed;
    snapshot->flush_started = g_gui_diag.flush_started;
    snapshot->flush_completed = g_gui_diag.flush_completed;
    snapshot->touch_started = g_gui_diag.touch_started;
    snapshot->touch_completed = g_gui_diag.touch_completed;
    snapshot->last_loop_tick = g_gui_diag.last_loop_tick;
    snapshot->last_flush_tick = g_gui_diag.last_flush_tick;
    snapshot->last_touch_tick = g_gui_diag.last_touch_tick;
    snapshot->last_flush_pixels = g_gui_diag.last_flush_pixels;
    snapshot->last_flush_x1 = g_gui_diag.last_flush_x1;
    snapshot->last_flush_y1 = g_gui_diag.last_flush_y1;
    snapshot->last_flush_x2 = g_gui_diag.last_flush_x2;
    snapshot->last_flush_y2 = g_gui_diag.last_flush_y2;
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *uart)
{
    if (uart == NULL) {
        return;
    }
    if (uart->Instance == USART1) {
        USART1_TxComplete();
    } else if (uart->Instance == UART4) {
        WiFi_UART_TxComplete();
    } else if (uart->Instance == USART3) {
        BT_UART_TxComplete();
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart)
{
    if (uart == NULL) {
        return;
    }
    if (uart->Instance == USART1) {
        if ((uart->ErrorCode & HAL_UART_ERROR_DMA) != 0U) {
            USART1_TxError();
        }
        USART1_ConsoleErrorCallback(uart);
    } else if (uart->Instance == UART4) {
        WiFi_UART_ErrorCallback(uart);
    } else if (uart->Instance == USART3) {
        BT_UART_ErrorCallback(uart);
    }
}
