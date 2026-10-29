#ifndef __USART_H
#define __USART_H

#include "stm32f4xx_hal.h"

typedef struct {
    uint32_t dma_started;
    uint32_t dma_start_failures;
    uint32_t dma_completed;
    uint32_t dma_error_callbacks;
    uint32_t failed_transmissions;
    uint32_t rejected_replies;
    uint32_t dropped_logs;
    uint32_t pending;
    uint8_t busy;
} USART1TxDiagnostic;

// USART1 handle
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart3;
extern UART_HandleTypeDef huart4;

// Initialize USART1 (PA9-TX, PA10-RX, 115200 baud, 8N1)
void USART1_Init(void);
void USART3_Init(void);
void UART4_Init(void);

// Send a single character
void USART1_SendChar(uint8_t ch);

// Queue low-priority output; all transmission is serialized through DMA.
void USART1_SendString(const char *str);
void USART1_SendReply(const char *str);
void USART1_TxProcess(void);
void USART1_TxComplete(void);
void USART1_TxError(void);
uint32_t USART1_GetDroppedLogCount(void);
uint32_t USART1_GetRejectedReplyCount(void);
uint32_t USART1_GetTxFailureCount(void);
void USART1_GetTxDiagnostic(USART1TxDiagnostic *diagnostic);

/* DMA 句柄 */
extern DMA_HandleTypeDef hdma_usart1_tx;

#endif
