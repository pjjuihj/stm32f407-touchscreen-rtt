#include "bluetooth.h"
#include "usart.h"

#include <assert.h>
#include <stddef.h>

GPIO_TypeDef test_gpiof;
GPIO_TypeDef test_uart4_instance;
GPIO_TypeDef test_usart3_instance;
UART_HandleTypeDef huart3 = { .Instance = USART3 };

static uint32_t g_state_pin;
static GPIO_InitTypeDef g_gpio_config;
static uint8_t *g_rx_target;
static unsigned int g_rx_callbacks;
static unsigned int g_rx_attempts;
static unsigned int g_rx_failures;

void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint32_t pin, uint32_t state)
{
    (void)port;
    (void)pin;
    (void)state;
}

void HAL_GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *init)
{
    assert(port == GPIOF);
    g_gpio_config = *init;
}

uint32_t HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint32_t pin)
{
    assert(port == GPIOF && pin == GPIO_PIN_13);
    return g_state_pin;
}

HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *uart, uint8_t *data,
                                     uint16_t length)
{
    assert(uart == &huart3 && length == 1U);
    g_rx_attempts++;
    if (g_rx_failures != 0U) {
        g_rx_failures--;
        return HAL_ERROR;
    }
    g_rx_target = data;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *uart, uint8_t *data,
                                      uint16_t length)
{
    (void)uart;
    (void)data;
    (void)length;
    return HAL_OK;
}

uint32_t HAL_GetTick(void) { return 0U; }
void USART3_Init(void) {}

static void OnByte(uint8_t byte)
{
    (void)byte;
    g_rx_callbacks++;
}

int main(void)
{
    BT_Init();
    assert(g_gpio_config.Pin == GPIO_PIN_13);
    assert(g_gpio_config.Mode == GPIO_MODE_INPUT);
    assert(g_state_pin == GPIO_PIN_RESET && !BT_IsConnected());
    g_state_pin = GPIO_PIN_SET;
    assert(BT_IsConnected());
    assert(BT_Connect());
    g_state_pin = GPIO_PIN_RESET;
    BT_Disconnect();
    assert(!BT_IsConnected());

    BT_SetRxCallback(OnByte);
    assert(g_rx_target != NULL);
    *g_rx_target = (uint8_t)'S';
    BT_UART_RxComplete(&huart3);
    assert(g_rx_callbacks == 0U);
    BT_Process();
    assert(g_rx_callbacks == 1U);
    for (unsigned int i = 0U; i < 40U; i++) {
        *g_rx_target = (uint8_t)'A';
        BT_UART_RxComplete(&huart3);
    }
    BT_Process();
    assert(g_rx_callbacks == 33U);
    BT_Process();
    assert(g_rx_callbacks == 41U);
    g_rx_failures = 1U;
    *g_rx_target = (uint8_t)'B';
    BT_UART_RxComplete(&huart3);
    {
        unsigned int attempts = g_rx_attempts;
        BT_Process();
        assert(g_rx_attempts == attempts + 1U);
    }
    g_rx_failures = 1U;
    g_rx_target = NULL;
    BT_Init();
    assert(g_rx_target == NULL);
    BT_Process();
    assert(g_rx_target != NULL);
    return 0;
}
