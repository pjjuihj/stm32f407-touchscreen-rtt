#include "board_io.h"
#include "stm32f4xx_hal.h"

static bool GetLedPin(uint8_t led, GPIO_TypeDef **port, uint16_t *pin)
{
    if (led == 0U) {
        *port = GPIOE;
        *pin = GPIO_PIN_3;
    } else if (led == 1U) {
        *port = GPIOE;
        *pin = GPIO_PIN_4;
    } else if (led == 2U) {
        *port = GPIOG;
        *pin = GPIO_PIN_9;
    } else {
        return false;
    }
    return true;
}

void LED_Set(uint8_t led, bool enabled)
{
    GPIO_TypeDef *port;
    uint16_t pin;
    if (GetLedPin(led, &port, &pin)) {
        HAL_GPIO_WritePin(port, pin, enabled ? GPIO_PIN_RESET : GPIO_PIN_SET);
    }
}

bool LED_Get(uint8_t led)
{
    GPIO_TypeDef *port;
    uint16_t pin;
    return GetLedPin(led, &port, &pin) && HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_RESET;
}

void BEEP_Set(bool enabled)
{
    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_7, enabled ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

bool BEEP_Get(void)
{
    return HAL_GPIO_ReadPin(GPIOG, GPIO_PIN_7) == GPIO_PIN_SET;
}
