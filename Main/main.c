#include "led.h"
#include "beep.h"
#include "key.h"
#include "lcd.h"
#include "touch.h"
#include "usart.h"
#include "log.h"
#include "test_log.h"
#include "gui_driver.h"     /* LVGL驱动接口 */
#include "lvgl.h"           /* LVGL头文件 */
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
                    USART1_SendString("TEST RUN        - Run all tests\r\n");
                    USART1_SendString("TEST WRITE      - Test log write\r\n");
                    USART1_SendString("TEST OVERFLOW   - Test buffer overflow\r\n");
                    USART1_SendString("TEST CLEAR      - Clear test logs\r\n");
                    USART1_SendString("HELP            - Show this help\r\n");
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
  HAL_Init();                    	//初始化HAL库
  Stm32_Clock_Init(336,8,2,7);  	//设置时钟,168Mhz
	delay_init();     //延迟函数初始化

	// Initialize USART1 FIRST - for debug output
	USART1_Init();
	delay_ms(100);  // Wait for USART to be ready

	// 简单测试：只输出串口信息
	USART1_SendString("A");  // 发送单个字符
	delay_ms(100);
	USART1_SendString("B");  // 发送单个字符
	delay_ms(100);
	USART1_SendString("UART OK\r\n");  // 发送字符串

	// LVGL 初始化
	gui_log_init();     // 初始化日志（必须在lv_init之前）
	lv_init();          // 初始化LVGL内核
	gui_disp_init();    // 初始化显示驱动
	gui_touch_init();   // 初始化触摸驱动
	gui_tick_init();    // 初始化时钟驱动

	// 创建通知中心UI (暂时禁用，文件不存在)
	// ui_notification_create(lv_screen_active());
	// ui_notification_show();

	USART1_SendString("LVGL init OK\r\n");

	/* 主循环 - 处理LVGL任务 */
	uint32_t last_led_tick = 0;
	while(1) {
		lv_task_handler();  // 处理LVGL事件和渲染
		gui_log_flush();    // 刷新LVGL日志到UART

		// LED 心跳指示（每500ms闪烁一次）
		uint32_t now = HAL_GetTick();
		if(now - last_led_tick >= 500) {
			LED0 = !LED0;
			last_led_tick = now;
		}
	}
}

