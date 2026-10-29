#ifndef MAIN_CONSOLE_H
#define MAIN_CONSOLE_H

#include "stm32f4xx_hal.h"

void Console_Init(void);
void Console_Process(void);
void Console_UART_RxComplete(UART_HandleTypeDef *uart);
void Console_UART_ErrorCallback(UART_HandleTypeDef *uart);

#endif
