#ifndef __USART_H
#define __USART_H

#include "stm32f4xx_hal.h"

// Initialize USART1 (PA9-TX, PA10-RX, 115200 baud, 8N1)
void USART1_Init(void);

// Send a single character
void USART1_SendChar(uint8_t ch);

// Send a string (blocking)
void USART1_SendString(const char *str);

#endif
