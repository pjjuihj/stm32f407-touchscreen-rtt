#ifndef TEST_GUI_FAKE_BLUETOOTH_H
#define TEST_GUI_FAKE_BLUETOOTH_H

#include "stm32f4xx_hal.h"

void BT_UART_TxComplete(void);
void BT_UART_ErrorCallback(UART_HandleTypeDef *uart);

#endif
