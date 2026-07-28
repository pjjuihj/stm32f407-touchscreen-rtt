#include "led.h"
#include "beep.h"
#include "key.h"
#include "lcd.h"
#include "touch.h"
#include "usart.h"
#include "log.h"
#include "test_log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// External USART1 handle
extern UART_HandleTypeDef huart1;

// Simple receive buffer
static uint8_t rxChar;
volatile uint8_t rxInterruptCalled = 0;

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

        // Echo character
        USART1_SendChar(rxChar);

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
	HAL_StatusTypeDef status = HAL_UART_Receive_IT(&huart1, &rxChar, 1);

	// Initialize log system AFTER USART is ready
	Log_Init();
	Log_Write(LOG_MODULE_SYSTEM, LOG_LEVEL_INFO, "System starting...");

 	LCD_Init();           //��ʼ��LCD FSMC�ӿں���ʾ����
	Log_Write(LOG_MODULE_SYSTEM, LOG_LEVEL_INFO, "LCD initialized");

	Touch_Init();				//��������ʼ��
	Log_Write(LOG_MODULE_SYSTEM, LOG_LEVEL_INFO, "Touch initialized");

 	BRUSH_COLOR=RED;    //��������Ϊ��ɫ 
	LCD_DisplayString(10,10,16,"Illuminati STM32");	
  LCD_DisplayString(20,40,24,"Author:Clever");
	LCD_DisplayString(30,80,24,"19.TOUCH TEST");

	delay_ms(1000);
	
 	Clear_Screen();	       //������
  if(lcd_id==0x9341)
	   R_Touch_test();     //������������Բ���
	else if(lcd_id==0x1963)
	   C_Touch_test(); 		 //������������Բ���

	// Debug loop
	while(1) {
		if (rxInterruptCalled) {
			USART1_SendString("Interrupt OK\r\n");
			rxInterruptCalled = 0;
		}
		delay_ms(100);
	}
}

