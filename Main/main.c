#include "led.h"
#include "beep.h"
#include "key.h"
#include "lcd.h"
#include "touch.h"
#include "usart.h"
#include "log.h"
#include "test_log.h"

// External USART1 handle (defined in usart.c)
extern UART_HandleTypeDef huart1;

// Command receive buffer
static char rxBuffer[64];
static uint8_t rxIndex = 0;

// Process received character
void ProcessRxChar(uint8_t ch)
{
    if (ch == '\r' || ch == '\n') {
        if (rxIndex > 0) {
            rxBuffer[rxIndex] = '\0';
            Log_ProcessCommand(rxBuffer);
            rxIndex = 0;
        }
    } else if (rxIndex < sizeof(rxBuffer) - 1) {
        rxBuffer[rxIndex++] = ch;
    }
}

// USART1 IRQ handler
void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}

// UART receive callback
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        ProcessRxChar(huart->pRxBuffPtr[0]);
        // Re-enable receive interrupt
        HAL_UART_Receive_IT(&huart1, huart->pRxBuffPtr, 1);
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
 	LCD_Init();           //��ʼ��LCD FSMC�ӿں���ʾ����
	Touch_Init();				//��������ʼ��

	// Initialize USART and Log system
	USART1_Init();
	Log_Init();

	// Enable USART1 NVIC and start receive interrupt
	HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(USART1_IRQn);
	uint8_t rxChar;
	HAL_UART_Receive_IT(&huart1, &rxChar, 1);

 	BRUSH_COLOR=RED;    //��������Ϊ��ɫ
	LCD_DisplayString(10,10,16,"Illuminati STM32");
  LCD_DisplayString(20,40,24,"Author:Clever");
	LCD_DisplayString(30,80,24,"19.TOUCH TEST");

	Log_Write(LOG_MODULE_SYSTEM, LOG_LEVEL_INFO, "System started");

	delay_ms(1000);

 	Clear_Screen();	       //������
  if(lcd_id==0x9341)
	   R_Touch_test();     //������������Բ���
	else if(lcd_id==0x1963)
	   C_Touch_test(); 		 //������������Բ���
}

