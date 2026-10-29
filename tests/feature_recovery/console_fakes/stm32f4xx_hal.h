#ifndef TEST_CONSOLE_STM32F4XX_HAL_H
#define TEST_CONSOLE_STM32F4XX_HAL_H

#include "../fakes/stm32f4xx_hal.h"

extern GPIO_TypeDef test_usart1_instance;
#define USART1 (&test_usart1_instance)

#endif
