#ifndef TEST_GUI_FAKE_WIFI_H
#define TEST_GUI_FAKE_WIFI_H

#include "stm32f4xx_hal.h"

void WiFi_UART_TxComplete(void);
void WiFi_UART_ErrorCallback(UART_HandleTypeDef *uart);

#endif
