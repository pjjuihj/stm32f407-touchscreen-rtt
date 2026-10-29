#include "led.h"
#include "beep.h"
#include "key.h"
#include "lcd.h"
#include "lcd_backlight.h"
#include "touch.h"
#include "usart.h"
#include "log.h"
#include "test_log.h"
#include "console.h"
#include "iot.h"
#include "wifi.h"
#include "bluetooth.h"
#include "gui_driver.h"
#include "lvgl.h"
#include "SEGGER_RTT.h"
#include "SEGGER_SYSVIEW.h"

#include <stdio.h>

static uint32_t g_reset_csr_at_boot;
static uint32_t g_led_off_at;
static uint8_t g_led_pulse_pending;
static volatile uint32_t g_usart1_rx_bytes;
static volatile uint32_t g_usart1_error_callbacks;
static volatile uint32_t g_usart1_error_flags;

static void ReportUsart1Diagnostics(void)
{
    static uint32_t last_report_tick;
    static uint32_t previous_rx_bytes;
    static uint32_t previous_error_callbacks;
    static uint32_t previous_error_flags;
    static USART1TxDiagnostic previous_tx;
    static uint8_t have_previous;
    USART1TxDiagnostic tx;
    uint32_t now = HAL_GetTick();
    uint32_t rx_bytes;
    uint32_t error_callbacks;
    uint32_t error_flags;
    char line[224];
    int length;

    if ((uint32_t)(now - last_report_tick) < 1000U) {
        return;
    }
    last_report_tick = now;

    rx_bytes = g_usart1_rx_bytes;
    error_callbacks = g_usart1_error_callbacks;
    error_flags = g_usart1_error_flags;
    USART1_GetTxDiagnostic(&tx);
    if (have_previous != 0U &&
        rx_bytes == previous_rx_bytes &&
        error_callbacks == previous_error_callbacks &&
        error_flags == previous_error_flags &&
        tx.dma_started == previous_tx.dma_started &&
        tx.dma_start_failures == previous_tx.dma_start_failures &&
        tx.dma_completed == previous_tx.dma_completed &&
        tx.dma_error_callbacks == previous_tx.dma_error_callbacks &&
        tx.failed_transmissions == previous_tx.failed_transmissions &&
        tx.rejected_replies == previous_tx.rejected_replies &&
        tx.dropped_logs == previous_tx.dropped_logs &&
        tx.pending == previous_tx.pending &&
        tx.busy == previous_tx.busy) {
        return;
    }

    previous_rx_bytes = rx_bytes;
    previous_error_callbacks = error_callbacks;
    previous_error_flags = error_flags;
    previous_tx = tx;
    have_previous = 1U;

    length = snprintf(line, sizeof(line),
                      "UART1_DIAG RX=%lu ERRCB=%lu ERR=0x%08lX "
                      "TXSTART=%lu TXDONE=%lu TXSTARTFAIL=%lu TXDMAERR=%lu "
                      "TXFAIL=%lu TXREJ=%lu TXDROP=%lu BUSY=%u PENDING=%lu\r\n",
                      (unsigned long)rx_bytes,
                      (unsigned long)error_callbacks,
                      (unsigned long)error_flags,
                      (unsigned long)tx.dma_started,
                      (unsigned long)tx.dma_completed,
                      (unsigned long)tx.dma_start_failures,
                      (unsigned long)tx.dma_error_callbacks,
                      (unsigned long)tx.failed_transmissions,
                      (unsigned long)tx.rejected_replies,
                      (unsigned long)tx.dropped_logs,
                      (unsigned int)tx.busy,
                      (unsigned long)tx.pending);
    if (length > 0 && (size_t)length < sizeof(line)) {
        (void)SEGGER_RTT_WriteString(RTT_CHANNEL_APP, line);
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
    if (uart == NULL) {
        return;
    }
    if (uart->Instance == USART1) {
        g_usart1_rx_bytes++;
        Console_UART_RxComplete(uart);
    } else if (uart->Instance == UART4) {
        WiFi_UART_RxComplete(uart);
    } else if (uart->Instance == USART3) {
        BT_UART_RxComplete(uart);
    }
}

void USART1_ConsoleErrorCallback(UART_HandleTypeDef *uart)
{
    if (uart == NULL || uart->Instance != USART1) {
        return;
    }
    g_usart1_error_callbacks++;
    g_usart1_error_flags |= uart->ErrorCode;
    Console_UART_ErrorCallback(uart);
}

static void ButtonEventCallback(lv_event_t *event)
{
    (void)event;
    LED0 = 0;
    g_led_off_at = HAL_GetTick() + 100U;
    g_led_pulse_pending = 1U;
}

int main(void)
{
    g_reset_csr_at_boot = RCC->CSR;

    HAL_Init();
    Stm32_Clock_Init(336, 8, 2, 7);
    delay_init();
    LED_Init();
    BEEP_Init();
    KEY_Init();

    USART1_Init();
    Console_Init();
    {
        char reset_message[40];
        (void)snprintf(reset_message, sizeof(reset_message),
                       "F407 RESET_CSR=0x%08lX\r\n",
                       (unsigned long)g_reset_csr_at_boot);
        USART1_SendReply(reset_message);
        USART1_TxProcess();
    }
    __HAL_RCC_CLEAR_RESET_FLAGS();
    SEGGER_RTT_Init();
    SEGGER_SYSVIEW_Init();
    SEGGER_SYSVIEW_Start();

    Log_Init();
    Log_Write(LOG_MODULE_SYSTEM, LOG_LEVEL_INFO, "System starting...");

    LCD_Backlight_Init();
    LCD_Init();
    Log_Write(LOG_MODULE_SYSTEM, LOG_LEVEL_INFO, "LCD initialized");
    Touch_Init();
    Log_Write(LOG_MODULE_SYSTEM, LOG_LEVEL_INFO, "Touch initialized");

    gui_log_init();
    lv_init();
    gui_tick_init();
    if (!gui_disp_init()) {
        Log_Write(LOG_MODULE_SYSTEM, LOG_LEVEL_ERROR,
                  "LVGL display initialization failed; Wi-Fi startup skipped");
        while (1) {
            Console_Process();
            gui_log_flush();
            USART1_TxProcess();
            ReportUsart1Diagnostics();
            delay_ms(5U);
        }
    }
    if (!gui_touch_init()) {
        Log_Write(LOG_MODULE_SYSTEM, LOG_LEVEL_ERROR,
                  "LVGL touch initialization failed");
    }
    Log_Write(LOG_MODULE_SYSTEM, LOG_LEVEL_INFO, "LVGL initialized");

    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, 0);

    lv_obj_t *title = lv_label_create(lv_screen_active());
    if (title != NULL) {
        lv_label_set_text(title, "LVGL Image Demo");
        lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    }

    lv_obj_t *home_button = lv_button_create(lv_screen_active());
    if (home_button != NULL) {
        lv_obj_set_size(home_button, 120, 50);
        lv_obj_align(home_button, LV_ALIGN_CENTER, 0, -40);
        lv_obj_add_event_cb(home_button, ButtonEventCallback, LV_EVENT_CLICKED, NULL);
        lv_obj_t *label = lv_label_create(home_button);
        if (label != NULL) {
            lv_label_set_text(label, LV_SYMBOL_HOME " Home");
            lv_obj_center(label);
        }
    }

    lv_obj_t *settings_button = lv_button_create(lv_screen_active());
    if (settings_button != NULL) {
        lv_obj_set_size(settings_button, 120, 50);
        lv_obj_align(settings_button, LV_ALIGN_CENTER, 0, 40);
        lv_obj_add_event_cb(settings_button, ButtonEventCallback, LV_EVENT_CLICKED, NULL);
        lv_obj_t *label = lv_label_create(settings_button);
        if (label != NULL) {
            lv_label_set_text(label, LV_SYMBOL_SETTINGS " Settings");
            lv_obj_center(label);
        }
    }

    /* Bring up the display before starting the asynchronous ESP sync. */
    lv_refr_now(NULL);
    IoT_Init(NULL);

    while (1) {
        if (g_led_pulse_pending != 0U &&
            (int32_t)(HAL_GetTick() - g_led_off_at) >= 0) {
            LED0 = 1;
            g_led_pulse_pending = 0U;
        }
        Console_Process();
        IoT_Process();
        gui_log_flush();
        USART1_TxProcess();
        ReportUsart1Diagnostics();
        lv_task_handler();
        gui_diag_note_loop_completed();
        delay_ms(5);
    }
}
