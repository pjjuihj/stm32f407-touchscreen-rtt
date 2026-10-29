#include "bluetooth.h"
#include "usart.h"

#include <string.h>

#define BT_RX_CAPACITY 128U
#define BT_TX_CAPACITY 256U
#define BT_RX_BUDGET 32U

static uint8_t g_bt_rx_byte;
static uint8_t g_bt_rx[BT_RX_CAPACITY];
static volatile uint16_t g_bt_rx_head;
static volatile uint16_t g_bt_rx_tail;
static uint8_t g_bt_tx[BT_TX_CAPACITY];
static volatile uint8_t g_bt_tx_busy;
static volatile uint8_t g_bt_rx_rearm_pending;
static BT_RxCallback g_bt_rx_callback;

static void RearmBtRx(void)
{
    g_bt_rx_rearm_pending = 0U;
    if (HAL_UART_Receive_IT(&huart3, &g_bt_rx_byte, 1U) != HAL_OK) {
        g_bt_rx_rearm_pending = 1U;
    }
}

void BT_Init(void)
{
    GPIO_InitTypeDef state_gpio = {0};

    g_bt_rx_head = 0U;
    g_bt_rx_tail = 0U;
    g_bt_tx_busy = 0U;
    g_bt_rx_rearm_pending = 0U;
    g_bt_rx_callback = NULL;
    __HAL_RCC_GPIOF_CLK_ENABLE();
    state_gpio.Pin = GPIO_PIN_13;
    state_gpio.Mode = GPIO_MODE_INPUT;
    state_gpio.Pull = GPIO_PULLDOWN;
    state_gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOF, &state_gpio);
    USART3_Init();
    RearmBtRx();
}

bool BT_Connect(void)
{
    return BT_IsConnected();
}

void BT_Disconnect(void)
{
    /* Link state is owned by the DX-BT05 STATE output, not by a local flag. */
}

bool BT_IsConnected(void)
{
    return HAL_GPIO_ReadPin(GPIOF, GPIO_PIN_13) == GPIO_PIN_SET;
}

void BT_SetRxCallback(BT_RxCallback callback)
{
    g_bt_rx_callback = callback;
}

void BT_UART_RxComplete(UART_HandleTypeDef *uart)
{
    uint16_t next;
    if (uart == NULL || uart->Instance != USART3) {
        return;
    }
    next = (uint16_t)((g_bt_rx_head + 1U) % BT_RX_CAPACITY);
    if (next != g_bt_rx_tail) {
        g_bt_rx[g_bt_rx_head] = g_bt_rx_byte;
        g_bt_rx_head = next;
    }
    RearmBtRx();
}

void BT_UART_TxComplete(void)
{
    g_bt_tx_busy = 0U;
    memset(g_bt_tx, 0, sizeof(g_bt_tx));
}

void BT_UART_ErrorCallback(UART_HandleTypeDef *uart)
{
    if (uart == NULL || uart->Instance != USART3) {
        return;
    }
    RearmBtRx();
}

void BT_Process(void)
{
    uint8_t processed = 0U;
    if (g_bt_rx_rearm_pending != 0U) {
        RearmBtRx();
    }
    while (g_bt_rx_tail != g_bt_rx_head && processed < BT_RX_BUDGET) {
        uint8_t byte = g_bt_rx[g_bt_rx_tail];
        g_bt_rx_tail = (uint16_t)((g_bt_rx_tail + 1U) % BT_RX_CAPACITY);
        if (g_bt_rx_callback != NULL) {
            g_bt_rx_callback(byte);
        }
        processed++;
    }
}

void BT_SendRaw(const uint8_t *data, uint16_t length)
{
    if (data == NULL || length == 0U || length > sizeof(g_bt_tx) || g_bt_tx_busy != 0U) {
        return;
    }
    memcpy(g_bt_tx, data, length);
    g_bt_tx_busy = 1U;
    if (HAL_UART_Transmit_IT(&huart3, g_bt_tx, length) == HAL_OK) {
        return;
    } else {
        g_bt_tx_busy = 0U;
        memset(g_bt_tx, 0, sizeof(g_bt_tx));
    }
}

void BT_SendData(const char *data)
{
    size_t length;
    if (data == NULL) {
        return;
    }
    length = strlen(data);
    if (length <= UINT16_MAX) {
        BT_SendRaw((const uint8_t *)data, (uint16_t)length);
    }
}
