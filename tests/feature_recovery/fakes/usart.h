#ifndef TEST_USART_H
#define TEST_USART_H

#include "stm32f4xx_hal.h"

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart3;
extern UART_HandleTypeDef huart4;
void UART4_Init(void);
void USART3_Init(void);
void USART1_SendString(const char *text);
void USART1_SendReply(const char *text);

#endif
