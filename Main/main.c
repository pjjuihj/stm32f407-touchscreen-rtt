#include "led.h"
#include "beep.h"
#include "key.h"
#include "lcd.h"
#include "touch.h"
#include "usart.h"
#include "log.h"
#include "test_log.h"
#include "gui_driver.h"     /* 新增: LVGL驱动接口 */
#include "lvgl.h"           /* 新增: LVGL头文件 */
#include "SEGGER_RTT.h"     /* SEGGER RTT debug output */
#include "SEGGER_SYSVIEW.h" /* SEGGER SystemView */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// External USART1 handle
extern UART_HandleTypeDef huart1;

// Simple receive buffer
static uint8_t rxChar;
volatile uint8_t rxInterruptCalled = 0;
static volatile uint8_t txPending = 0;  // 非阻塞回声标志

// Command buffer (increased to 64 bytes)
static char cmdBuffer[64];
static uint8_t cmdIndex = 0;

// USART1 IRQ handler
void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}

// UART receive callback
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        rxInterruptCalled = 1;

        // Echo character: 保存待发送字符，在主循环中发送（避免中断阻塞）
        txPending = rxChar;

        // Process character
        if (rxChar == '\r' || rxChar == '\n') {
            // End of command
            if (cmdIndex > 0) {
                cmdBuffer[cmdIndex] = '\0';
                // Process command
                if (strcmp(cmdBuffer, "STATUS") == 0) {
                    USART1_SendString("\r\n--- Log Status ---\r\n");
                    for (int i = 0; i < LOG_MODULE_MAX; i++) {
                        char buf[32];
                        snprintf(buf, sizeof(buf), "%s: %s\r\n",
                                 LogModuleNames[i],
                                 g_logConfig.enabled[i] ? "ON" : "OFF");
                        USART1_SendString(buf);
                    }
                } else if (strcmp(cmdBuffer, "TOUCH ON") == 0) {
                    Log_Enable(LOG_MODULE_TOUCH, true);
                    USART1_SendString("\r\nTouch log: ON\r\n");
                } else if (strcmp(cmdBuffer, "TOUCH OFF") == 0) {
                    Log_Enable(LOG_MODULE_TOUCH, false);
                    USART1_SendString("\r\nTouch log: OFF\r\n");
                } else if (strcmp(cmdBuffer, "LCD ON") == 0) {
                    Log_Enable(LOG_MODULE_LCD, true);
                    USART1_SendString("\r\nLCD log: ON\r\n");
                } else if (strcmp(cmdBuffer, "LCD OFF") == 0) {
                    Log_Enable(LOG_MODULE_LCD, false);
                    USART1_SendString("\r\nLCD log: OFF\r\n");
                } else if (strcmp(cmdBuffer, "SYSTEM ON") == 0) {
                    Log_Enable(LOG_MODULE_SYSTEM, true);
                    USART1_SendString("\r\nSystem log: ON\r\n");
                } else if (strcmp(cmdBuffer, "SYSTEM OFF") == 0) {
                    Log_Enable(LOG_MODULE_SYSTEM, false);
                    USART1_SendString("\r\nSystem log: OFF\r\n");
                } else if (strcmp(cmdBuffer, "LEVEL TOUCH 1") == 0) {
                    Log_SetLevel(LOG_MODULE_TOUCH, 1);
                    USART1_SendString("\r\nTouch level: ERROR\r\n");
                } else if (strcmp(cmdBuffer, "LEVEL TOUCH 2") == 0) {
                    Log_SetLevel(LOG_MODULE_TOUCH, 2);
                    USART1_SendString("\r\nTouch level: WARNING\r\n");
                } else if (strcmp(cmdBuffer, "LEVEL TOUCH 3") == 0) {
                    Log_SetLevel(LOG_MODULE_TOUCH, 3);
                    USART1_SendString("\r\nTouch level: INFO\r\n");
                } else if (strcmp(cmdBuffer, "LEVEL TOUCH 4") == 0) {
                    Log_SetLevel(LOG_MODULE_TOUCH, 4);
                    USART1_SendString("\r\nTouch level: DEBUG\r\n");
                } else if (strcmp(cmdBuffer, "LEVEL LCD 1") == 0) {
                    Log_SetLevel(LOG_MODULE_LCD, 1);
                    USART1_SendString("\r\nLCD level: ERROR\r\n");
                } else if (strcmp(cmdBuffer, "LEVEL LCD 2") == 0) {
                    Log_SetLevel(LOG_MODULE_LCD, 2);
                    USART1_SendString("\r\nLCD level: WARNING\r\n");
                } else if (strcmp(cmdBuffer, "LEVEL LCD 3") == 0) {
                    Log_SetLevel(LOG_MODULE_LCD, 3);
                    USART1_SendString("\r\nLCD level: INFO\r\n");
                } else if (strcmp(cmdBuffer, "LEVEL LCD 4") == 0) {
                    Log_SetLevel(LOG_MODULE_LCD, 4);
                    USART1_SendString("\r\nLCD level: DEBUG\r\n");
                } else if (strcmp(cmdBuffer, "LEVEL SYSTEM 1") == 0) {
                    Log_SetLevel(LOG_MODULE_SYSTEM, 1);
                    USART1_SendString("\r\nSystem level: ERROR\r\n");
                } else if (strcmp(cmdBuffer, "LEVEL SYSTEM 2") == 0) {
                    Log_SetLevel(LOG_MODULE_SYSTEM, 2);
                    USART1_SendString("\r\nSystem level: WARNING\r\n");
                } else if (strcmp(cmdBuffer, "LEVEL SYSTEM 3") == 0) {
                    Log_SetLevel(LOG_MODULE_SYSTEM, 3);
                    USART1_SendString("\r\nSystem level: INFO\r\n");
                } else if (strcmp(cmdBuffer, "LEVEL SYSTEM 4") == 0) {
                    Log_SetLevel(LOG_MODULE_SYSTEM, 4);
                    USART1_SendString("\r\nSystem level: DEBUG\r\n");
                } else if (strcmp(cmdBuffer, "DUMP") == 0) {
                    Log_Dump();
                } else if (strcmp(cmdBuffer, "CLEAR") == 0) {
                    Log_Clear();
                    USART1_SendString("\r\nLog cleared\r\n");
                } else if (strncmp(cmdBuffer, "TEST ", 5) == 0) {
                    // TEST command
                    char *test_cmd = cmdBuffer + 5;
                    if (strcmp(test_cmd, "RUN") == 0) {
                        Test_RunAll();
                    } else if (strcmp(test_cmd, "WRITE") == 0) {
                        for (int i = 0; i < 5; i++) {
                            Log_Write(LOG_MODULE_SYSTEM, LOG_LEVEL_INFO, "Test write %d", i);
                        }
                        USART1_SendString("\r\nTest write complete\r\n");
                    } else if (strcmp(test_cmd, "OVERFLOW") == 0) {
                        for (int i = 0; i < 70; i++) {
                            Log_Write(LOG_MODULE_SYSTEM, LOG_LEVEL_INFO, "Overflow %d", i);
                        }
                        USART1_SendString("\r\nTest overflow complete\r\n");
                    } else if (strcmp(test_cmd, "CLEAR") == 0) {
                        Log_Clear();
                        USART1_SendString("\r\nTest log cleared\r\n");
                    } else {
                        USART1_SendString("\r\nUnknown TEST command. Use: RUN/WRITE/OVERFLOW/CLEAR\r\n");
                    }
                } else if (strcmp(cmdBuffer, "HELP") == 0) {
                    USART1_SendString("\r\n--- Commands ---\r\n");
                    USART1_SendString("STATUS          - Show log status\r\n");
                    USART1_SendString("TOUCH ON/OFF    - Enable/disable touch log\r\n");
                    USART1_SendString("LCD ON/OFF      - Enable/disable LCD log\r\n");
                    USART1_SendString("SYSTEM ON/OFF   - Enable/disable system log\r\n");
                    USART1_SendString("LEVEL <MOD> <1-4> - Set log level\r\n");
                    USART1_SendString("DUMP            - Dump all logs\r\n");
                    USART1_SendString("CLEAR           - Clear log buffer\r\n");
                    USART1_SendString("LOGMODE RTT     - Switch log output to RTT\r\n");
                    USART1_SendString("LOGMODE UART    - Switch log output to UART\r\n");
                    USART1_SendString("TEST RUN        - Run all tests\r\n");
                    USART1_SendString("TEST WRITE      - Test log write\r\n");
                    USART1_SendString("TEST OVERFLOW   - Test buffer overflow\r\n");
                    USART1_SendString("TEST CLEAR      - Clear test logs\r\n");
                    USART1_SendString("HELP            - Show this help\r\n");
                } else if (strcmp(cmdBuffer, "LOGMODE RTT") == 0) {
                    gui_log_set_output_mode(LOG_OUTPUT_RTT);
                    USART1_SendString("\r\nLog output: RTT\r\n");
                } else if (strcmp(cmdBuffer, "LOGMODE UART") == 0) {
                    gui_log_set_output_mode(LOG_OUTPUT_UART);
                    USART1_SendString("\r\nLog output: UART\r\n");
                } else {
                    USART1_SendString("\r\nUnknown command. Type HELP.\r\n");
                }
                cmdIndex = 0;
            }
        } else if (cmdIndex < sizeof(cmdBuffer) - 1) {
            cmdBuffer[cmdIndex++] = rxChar;
        }

        // Re-enable receive interrupt
        HAL_UART_Receive_IT(&huart1, &rxChar, 1);
    }
}

/*********************************************************************************
*********************�������� STM32F407Ӧ�ÿ�����(�����)*************************
**********************************************************************************
* �ļ�����: ����3 ����ʹ��������main()                                           *
* �ļ�����������ʵ��                                                             *
* �������ڣ�2017.08.30                                                           *
* ��    ����V1.0                                                                 *
* ��    �ߣ�Clever                                                               *
* ˵    ������������LED���������������                                          *
* �Ա����̣�https://shop125046348.taobao.com                                     *
* ��    ���������̴��������ѧϰ�ο�                                             *
 **********************************************************************************
 *********************************************************************************/

/* 按钮点击事件回调 - LED闪烁 */
static void btn_event_cb(lv_event_t *e)
{
	LED0 = 0;   /* 亮 */
	delay_ms(100);
	LED0 = 1;   /* 灭 */
}

int main(void)
{
  HAL_Init();                    	//��ʼ��HAL��
  Stm32_Clock_Init(336,8,2,7);  	//����ʱ��,168Mhz
	delay_init();     //��ʱ������ʼ��
	LED_Init();				//LED��ʼ��
	BEEP_Init();      //��������ʼ��
	KEY_Init();       //������ʼ��

	// Initialize USART1 FIRST - for debug output
	USART1_Init();
	delay_ms(100);  // Wait for USART to be ready

	// Enable USART1 receive interrupt
	HAL_UART_Receive_IT(&huart1, &rxChar, 1);

	// Initialize SEGGER RTT for debug output (before any logging)
	SEGGER_RTT_Init();

	// Initialize SystemView for real-time analysis
	SEGGER_SYSVIEW_Init();
	SEGGER_SYSVIEW_Start();

	// Initialize log system AFTER USART is ready
	Log_Init();
	Log_Write(LOG_MODULE_SYSTEM, LOG_LEVEL_INFO, "System starting...");

	/* 初始化LCD */
	LCD_Init();
	Log_Write(LOG_MODULE_SYSTEM, LOG_LEVEL_INFO, "LCD initialized");

	/* 初始化触摸 */
	Touch_Init();
	Log_Write(LOG_MODULE_SYSTEM, LOG_LEVEL_INFO, "Touch initialized");

	/* LVGL初始化 - 日志必须在lv_init之前注册 */
	gui_log_init();
	lv_init();
	gui_tick_init();
	gui_disp_init();
	gui_touch_init();
	Log_Write(LOG_MODULE_SYSTEM, LOG_LEVEL_INFO, "LVGL initialized");

	/* 设置屏幕背景色 */
	lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0xFFFFFF), 0);
	lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, 0);

	/* 创建标题标签 */
	lv_obj_t *title = lv_label_create(lv_screen_active());
	if(title != NULL)
	{
		lv_label_set_text(title, "LVGL Image Demo");
		lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
		lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
	}

	/* 创建带图标的按钮1 - 使用LVGL符号 */
	lv_obj_t *btn1 = lv_button_create(lv_screen_active());
	if(btn1 != NULL)
	{
		lv_obj_set_size(btn1, 120, 50);
		lv_obj_align(btn1, LV_ALIGN_CENTER, 0, -40);
		lv_obj_add_event_cb(btn1, btn_event_cb, LV_EVENT_CLICKED, NULL);

		lv_obj_t *label1 = lv_label_create(btn1);
		if(label1 != NULL)
		{
			/* LV_SYMBOL_HOME 是LVGL内置的主页图标 */
			lv_label_set_text(label1, LV_SYMBOL_HOME " Home");
			lv_obj_center(label1);
		}
	}

	/* 创建带图标的按钮2 - 使用LVGL符号 */
	lv_obj_t *btn2 = lv_button_create(lv_screen_active());
	if(btn2 != NULL)
	{
		lv_obj_set_size(btn2, 120, 50);
		lv_obj_align(btn2, LV_ALIGN_CENTER, 0, 40);
		lv_obj_add_event_cb(btn2, btn_event_cb, LV_EVENT_CLICKED, NULL);

		lv_obj_t *label2 = lv_label_create(btn2);
		if(label2 != NULL)
		{
			/* LV_SYMBOL_SETTINGS 是LVGL内置的设置图标 */
			lv_label_set_text(label2, LV_SYMBOL_SETTINGS " Settings");
			lv_obj_center(label2);
		}
	}

	/* 强制刷新整个屏幕 */
	lv_refr_now(NULL);

	/* 主循环 */
	while(1) {
		/* 非阻塞回声: 在主循环中发送，避免中断阻塞 */
		if(txPending != 0) {
			USART1_SendChar(txPending);
			txPending = 0;
		}
		/* LVGL日志: 从缓冲区发送到串口 */
		gui_log_flush();
		lv_task_handler();
		delay_ms(5);
	}
}

