#ifndef TEST_GUI_FAKE_STM32F4XX_HAL_H
#define TEST_GUI_FAKE_STM32F4XX_HAL_H

#include <stdint.h>

typedef struct {
    uint32_t Instance;
    uint32_t ErrorCode;
} UART_HandleTypeDef;

#define USART1 1U
#define UART4  4U
#define USART3 3U
#define HAL_UART_ERROR_DMA 0x10U

uint32_t HAL_GetTick(void);

#endif
