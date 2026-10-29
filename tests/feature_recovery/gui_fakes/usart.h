#ifndef TEST_GUI_FAKE_USART_H
#define TEST_GUI_FAKE_USART_H

#include "stm32f4xx_hal.h"

void USART1_SendString(const char *message);
void USART1_TxProcess(void);
void USART1_TxComplete(void);
void USART1_TxError(void);

#endif
