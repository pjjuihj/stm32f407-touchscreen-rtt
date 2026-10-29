#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#include "stm32f4xx_hal.h"

#include <stdbool.h>
#include <stdint.h>

typedef void (*BT_RxCallback)(uint8_t byte);

void BT_Init(void);
bool BT_Connect(void);
void BT_Disconnect(void);
bool BT_IsConnected(void);
void BT_Process(void);
void BT_SendData(const char *data);
void BT_SendRaw(const uint8_t *data, uint16_t length);
void BT_SetRxCallback(BT_RxCallback callback);
void BT_UART_RxComplete(UART_HandleTypeDef *uart);
void BT_UART_TxComplete(void);
void BT_UART_ErrorCallback(UART_HandleTypeDef *uart);

#endif
