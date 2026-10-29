#ifndef TEST_STM32F4XX_HAL_H
#define TEST_STM32F4XX_HAL_H

#include <stdint.h>

typedef struct { void *Instance; uint32_t ErrorCode; } UART_HandleTypeDef;
typedef struct { uint32_t Pin; uint32_t Mode; uint32_t Pull; uint32_t Speed; uint32_t Alternate; } GPIO_InitTypeDef;
typedef struct { uint32_t marker; } GPIO_TypeDef;
typedef enum { HAL_OK = 0, HAL_ERROR = 1 } HAL_StatusTypeDef;

extern GPIO_TypeDef test_gpiof;
extern GPIO_TypeDef test_uart4_instance;
extern GPIO_TypeDef test_usart3_instance;
#define GPIOF (&test_gpiof)
#define UART4 (&test_uart4_instance)
#define USART3 (&test_usart3_instance)
#define GPIO_PIN_14 0x4000U
#define GPIO_PIN_13 0x2000U
#define GPIO_MODE_OUTPUT_PP 1U
#define GPIO_MODE_INPUT 2U
#define GPIO_PULLUP 1U
#define GPIO_PULLDOWN 2U
#define GPIO_SPEED_FREQ_LOW 1U
#define GPIO_PIN_SET 1U
#define GPIO_PIN_RESET 0U
#define HAL_UART_ERROR_ORE 0x08U

#define __HAL_RCC_GPIOF_CLK_ENABLE() ((void)0)
#define __HAL_UART_CLEAR_OREFLAG(uart) ((void)(uart))
#define __get_PRIMASK() (0U)
#define __disable_irq() ((void)0)
#define __set_PRIMASK(mask) ((void)(mask))

void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint32_t pin, uint32_t state);
void HAL_GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *init);
uint32_t HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint32_t pin);
HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *uart, uint8_t *data,
                                      uint16_t length);
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *uart, uint8_t *data,
                                     uint16_t length);
uint32_t HAL_GetTick(void);
HAL_StatusTypeDef HAL_UART_AbortTransmit(UART_HandleTypeDef *uart);

#endif
