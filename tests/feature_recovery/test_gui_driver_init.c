#include "gui_driver.h"
#include "stm32f4xx_hal.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

static lv_display_t g_display_storage;
static lv_indev_t g_indev_storage;
static lv_display_t *g_display_create_result;
static lv_display_t *g_default_display;
static lv_indev_t *g_indev_create_result;
static unsigned int g_flush_cb_calls;
static unsigned int g_buffers_calls;
static unsigned int g_indev_type_calls;
static unsigned int g_indev_read_cb_calls;
static unsigned int g_indev_display_calls;

static int Check(int condition, const char *description)
{
    if (!condition) {
        (void)fprintf(stderr, "FAIL: %s\n", description);
        return 1;
    }
    return 0;
}

lv_display_t *lv_display_create(int32_t horizontal, int32_t vertical)
{
    (void)horizontal;
    (void)vertical;
    if (g_display_create_result != NULL) {
        g_default_display = g_display_create_result;
    }
    return g_display_create_result;
}

void lv_display_set_flush_cb(lv_display_t *display, lv_display_flush_cb_t callback)
{
    (void)display;
    if (callback != NULL) {
        g_flush_cb_calls++;
    }
}

void lv_display_set_buffers(lv_display_t *display, void *buffer_a, void *buffer_b,
                            uint32_t buffer_size, lv_display_render_mode_t mode)
{
    (void)display;
    (void)buffer_a;
    (void)buffer_b;
    (void)buffer_size;
    (void)mode;
    g_buffers_calls++;
}

lv_indev_t *lv_indev_create(void)
{
    return g_indev_create_result;
}

void lv_indev_set_type(lv_indev_t *input, lv_indev_type_t type)
{
    (void)input;
    (void)type;
    g_indev_type_calls++;
}

void lv_indev_set_read_cb(lv_indev_t *input, lv_indev_read_cb_t callback)
{
    (void)input;
    if (callback != NULL) {
        g_indev_read_cb_calls++;
    }
}

lv_display_t *lv_display_get_default(void)
{
    return g_default_display;
}

void lv_indev_set_display(lv_indev_t *input, lv_display_t *display)
{
    (void)input;
    if (display != NULL) {
        g_indev_display_calls++;
    }
}

void lv_display_flush_ready(lv_display_t *display)
{
    (void)display;
}

void lv_tick_set_cb(uint32_t (*callback)(void))
{
    (void)callback;
}

void lv_log_register_print_cb(lv_log_print_g_cb_t callback)
{
    (void)callback;
}

uint32_t HAL_GetTick(void)
{
    return 0U;
}

volatile uint16_t LCD_DATA;
uint16_t Xdown = 0xffffU;
uint16_t Ydown = 0xffffU;

void LCD_Open_Window(int16_t x, int16_t y, uint16_t width, uint16_t height)
{
    (void)x;
    (void)y;
    (void)width;
    (void)height;
}

void LCD_WriteGRAM(void) {}
void XPT2046_Scan(uint8_t mode) { (void)mode; }
void USART1_SendString(const char *message) { (void)message; }
void USART1_TxProcess(void) {}
void USART1_TxComplete(void) {}
void USART1_TxError(void) {}
void USART1_ConsoleErrorCallback(UART_HandleTypeDef *uart) { (void)uart; }
void WiFi_UART_TxComplete(void) {}
void WiFi_UART_ErrorCallback(UART_HandleTypeDef *uart) { (void)uart; }
void BT_UART_TxComplete(void) {}
void BT_UART_ErrorCallback(UART_HandleTypeDef *uart) { (void)uart; }
int SEGGER_RTT_WriteString(unsigned int channel, const char *message)
{
    (void)channel;
    (void)message;
    return 0;
}

int main(void)
{
    int failed = 0;

    g_display_create_result = NULL;
    failed |= Check(!gui_disp_init(), "display allocation failure is reported");
    failed |= Check(g_flush_cb_calls == 0U && g_buffers_calls == 0U,
                    "display callbacks and buffers are not installed after allocation failure");

    g_display_create_result = &g_display_storage;
    failed |= Check(gui_disp_init(), "successful display allocation is reported");
    failed |= Check(g_flush_cb_calls == 1U && g_buffers_calls == 1U,
                    "successful display allocation installs flush callback and buffers");

    g_indev_create_result = NULL;
    failed |= Check(!gui_touch_init(), "input-device allocation failure is reported");
    failed |= Check(g_indev_type_calls == 0U && g_indev_read_cb_calls == 0U,
                    "input callbacks are not installed after allocation failure");

    g_indev_create_result = &g_indev_storage;
    failed |= Check(gui_touch_init(), "successful input-device allocation is reported");
    failed |= Check(g_indev_type_calls == 1U && g_indev_read_cb_calls == 1U &&
                    g_indev_display_calls == 1U,
                    "successful input-device allocation is configured and attached");

    if (failed != 0) {
        return 1;
    }
    (void)puts("gui driver init: allocation failures propagated");
    return 0;
}
