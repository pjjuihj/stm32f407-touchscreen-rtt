#include "usart.h"
#include "usart1_tx_scheduler.h"

#include <string.h>

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart3;
UART_HandleTypeDef huart4;
DMA_HandleTypeDef hdma_usart1_tx;

static USART1TxScheduler g_usart1_tx;
static volatile uint32_t g_usart1_dma_started;
static volatile uint32_t g_usart1_dma_start_failures;
static volatile uint32_t g_usart1_dma_completed;
static volatile uint32_t g_usart1_dma_error_callbacks;

static void ConfigureAfPin(GPIO_TypeDef *port, uint32_t pin, uint32_t alternate)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = pin;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = alternate;
    HAL_GPIO_Init(port, &gpio);
}

void HAL_UART_MspInit(UART_HandleTypeDef *uart)
{
    if (uart->Instance == USART1) {
        __HAL_RCC_USART1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();
        ConfigureAfPin(GPIOA, GPIO_PIN_9, GPIO_AF7_USART1);
        ConfigureAfPin(GPIOA, GPIO_PIN_10, GPIO_AF7_USART1);

        __HAL_RCC_DMA2_CLK_ENABLE();
        hdma_usart1_tx.Instance = DMA2_Stream7;
        hdma_usart1_tx.Init.Channel = DMA_CHANNEL_4;
        hdma_usart1_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
        hdma_usart1_tx.Init.PeriphInc = DMA_PINC_DISABLE;
        hdma_usart1_tx.Init.MemInc = DMA_MINC_ENABLE;
        hdma_usart1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_usart1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        hdma_usart1_tx.Init.Mode = DMA_NORMAL;
        hdma_usart1_tx.Init.Priority = DMA_PRIORITY_LOW;
        hdma_usart1_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
        if (HAL_DMA_Init(&hdma_usart1_tx) == HAL_OK) {
            __HAL_LINKDMA(uart, hdmatx, hdma_usart1_tx);
            HAL_NVIC_SetPriority(DMA2_Stream7_IRQn, 5, 0);
            HAL_NVIC_EnableIRQ(DMA2_Stream7_IRQn);
        }
        HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(USART1_IRQn);
    } else if (uart->Instance == USART3) {
        __HAL_RCC_USART3_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();
        ConfigureAfPin(GPIOB, GPIO_PIN_10 | GPIO_PIN_11, GPIO_AF7_USART3);
        HAL_NVIC_SetPriority(USART3_IRQn, 6, 0);
        HAL_NVIC_EnableIRQ(USART3_IRQn);
    } else if (uart->Instance == UART4) {
        __HAL_RCC_UART4_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();
        ConfigureAfPin(GPIOA, GPIO_PIN_0 | GPIO_PIN_1, GPIO_AF8_UART4);
        HAL_NVIC_SetPriority(UART4_IRQn, 6, 0);
        HAL_NVIC_EnableIRQ(UART4_IRQn);
    }
}

static void InitializeUart(UART_HandleTypeDef *uart, USART_TypeDef *instance,
                           uint32_t baud_rate)
{
    uart->Instance = instance;
    uart->Init.BaudRate = baud_rate;
    uart->Init.WordLength = UART_WORDLENGTH_8B;
    uart->Init.StopBits = UART_STOPBITS_1;
    uart->Init.Parity = UART_PARITY_NONE;
    uart->Init.Mode = UART_MODE_TX_RX;
    uart->Init.HwFlowCtl = UART_HWCONTROL_NONE;
    uart->Init.OverSampling = UART_OVERSAMPLING_16;
    (void)HAL_UART_Init(uart);
}

void USART1_Init(void)
{
    USART1TxScheduler_Init(&g_usart1_tx);
    InitializeUart(&huart1, USART1, 115200U);
}

void USART3_Init(void)
{
    InitializeUart(&huart3, USART3, 9600U);
}

void UART4_Init(void)
{
    InitializeUart(&huart4, UART4, 115200U);
}

static int StartUsart1Dma(void *context, const uint8_t *data, size_t length)
{
    HAL_StatusTypeDef status;

    (void)context;
    status = HAL_UART_Transmit_DMA(&huart1, (uint8_t *)data, (uint16_t)length);
    if (status == HAL_OK) {
        g_usart1_dma_started++;
        return 1;
    }
    g_usart1_dma_start_failures++;
    return 0;
}

static USART1TxResult QueueChunk(USART1TxPriority priority, const void *data, size_t length)
{
    uint32_t primask = __get_PRIMASK();
    USART1TxResult result;
    __disable_irq();
    result = USART1TxScheduler_Enqueue(&g_usart1_tx, priority, data, length);
    __set_PRIMASK(primask);
    return result;
}

static void QueueString(USART1TxPriority priority, const char *string)
{
    size_t remaining;

    if (string == NULL) {
        return;
    }
    remaining = strlen(string);
    while (remaining > 0U) {
        size_t chunk = remaining;
        if (chunk >= USART1_TX_MESSAGE_CAPACITY) {
            chunk = USART1_TX_MESSAGE_CAPACITY - 1U;
        }
        if (QueueChunk(priority, string, chunk) != USART1_TX_OK) {
            return;
        }
        string += chunk;
        remaining -= chunk;
    }
}

void USART1_SendChar(uint8_t character)
{
    (void)QueueChunk(USART1_TX_LOG, &character, 1U);
}

void USART1_SendString(const char *string)
{
    QueueString(USART1_TX_LOG, string);
}

void USART1_SendReply(const char *string)
{
    QueueString(USART1_TX_REPLY, string);
}

void USART1_TxProcess(void)
{
    (void)USART1TxScheduler_Process(&g_usart1_tx, StartUsart1Dma, NULL);
}

void USART1_TxComplete(void)
{
    g_usart1_dma_completed++;
    USART1TxScheduler_Complete(&g_usart1_tx);
}

void USART1_TxError(void)
{
    g_usart1_dma_error_callbacks++;
    USART1TxScheduler_Fail(&g_usart1_tx);
}

uint32_t USART1_GetDroppedLogCount(void)
{
    return g_usart1_tx.dropped_logs;
}

uint32_t USART1_GetRejectedReplyCount(void)
{
    return g_usart1_tx.rejected_replies;
}

uint32_t USART1_GetTxFailureCount(void)
{
    return g_usart1_tx.failed_transmissions;
}

void USART1_GetTxDiagnostic(USART1TxDiagnostic *diagnostic)
{
    uint32_t primask;
    size_t i;

    if (diagnostic == NULL) {
        return;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    diagnostic->dma_started = g_usart1_dma_started;
    diagnostic->dma_start_failures = g_usart1_dma_start_failures;
    diagnostic->dma_completed = g_usart1_dma_completed;
    diagnostic->dma_error_callbacks = g_usart1_dma_error_callbacks;
    diagnostic->failed_transmissions = g_usart1_tx.failed_transmissions;
    diagnostic->rejected_replies = g_usart1_tx.rejected_replies;
    diagnostic->dropped_logs = g_usart1_tx.dropped_logs;
    diagnostic->busy = g_usart1_tx.busy;
    diagnostic->pending = 0U;
    for (i = 0U; i < USART1_TX_SLOT_COUNT; i++) {
        if (g_usart1_tx.slots[i].occupied != 0U) {
            diagnostic->pending++;
        }
    }
    __set_PRIMASK(primask);
}

void DMA2_Stream7_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart1_tx);
}
