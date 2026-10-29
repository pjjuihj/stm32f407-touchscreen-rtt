#include "usart1_tx_scheduler.h"

#include <assert.h>
#include <string.h>

typedef struct {
    char sent[8][USART1_TX_MESSAGE_CAPACITY];
    size_t lengths[8];
    size_t count;
    int accept;
} Capture;

static int capture_start(void *context, const uint8_t *data, size_t length)
{
    Capture *capture = (Capture *)context;
    assert(capture->count < 8U);
    memcpy(capture->sent[capture->count], data, length);
    capture->sent[capture->count][length] = '\0';
    capture->lengths[capture->count] = length;
    capture->count++;
    return capture->accept;
}

static int complete_during_start(void *context, const uint8_t *data, size_t length)
{
    (void)data;
    (void)length;
    USART1TxScheduler_Complete((USART1TxScheduler *)context);
    return 1;
}

int main(void)
{
    USART1TxScheduler scheduler;
    Capture capture = { .accept = 1 };
    unsigned int i;

    USART1TxScheduler_Init(&scheduler);
    assert(USART1TxScheduler_Enqueue(&scheduler, USART1_TX_LOG, "log", 3U) == USART1_TX_OK);
    assert(USART1TxScheduler_Process(&scheduler, capture_start, &capture));
    assert(capture.count == 1U && strcmp(capture.sent[0], "log") == 0);
    assert(USART1TxScheduler_Enqueue(&scheduler, USART1_TX_LOG, "later-log", 9U) == USART1_TX_OK);
    assert(USART1TxScheduler_Enqueue(&scheduler, USART1_TX_REPLY, "WIFI STATUS\r\n", 13U) == USART1_TX_OK);
    assert(!USART1TxScheduler_Process(&scheduler, capture_start, &capture));
    assert(capture.count == 1U);

    USART1TxScheduler_Complete(&scheduler);
    assert(USART1TxScheduler_Process(&scheduler, capture_start, &capture));
    assert(capture.count == 2U && strcmp(capture.sent[1], "WIFI STATUS\r\n") == 0);
    USART1TxScheduler_Complete(&scheduler);
    assert(USART1TxScheduler_Process(&scheduler, capture_start, &capture));
    assert(capture.count == 3U && strcmp(capture.sent[2], "later-log") == 0);
    USART1TxScheduler_Complete(&scheduler);

    USART1TxScheduler_Init(&scheduler);
    for (i = 0; i < USART1_TX_REPLY_CAPACITY; i++) {
        assert(USART1TxScheduler_Enqueue(&scheduler, USART1_TX_REPLY, "r", 1U) == USART1_TX_OK);
    }
    assert(USART1TxScheduler_Enqueue(&scheduler, USART1_TX_REPLY, "overflow", 8U) == USART1_TX_FULL);
    assert(scheduler.rejected_replies == 1U);
    for (i = 0; i < USART1_TX_LOG_CAPACITY; i++) {
        assert(USART1TxScheduler_Enqueue(&scheduler, USART1_TX_LOG, "l", 1U) == USART1_TX_OK);
    }
    assert(USART1TxScheduler_Enqueue(&scheduler, USART1_TX_LOG, "drop", 4U) == USART1_TX_FULL);
    assert(scheduler.dropped_logs == 1U);

    USART1TxScheduler_Init(&scheduler);
    capture.count = 0U;
    capture.accept = 0;
    assert(USART1TxScheduler_Enqueue(&scheduler, USART1_TX_REPLY, "retry", 5U) == USART1_TX_OK);
    assert(!USART1TxScheduler_Process(&scheduler, capture_start, &capture));
    assert(!scheduler.busy);
    capture.accept = 1;
    assert(USART1TxScheduler_Process(&scheduler, capture_start, &capture));
    assert(capture.count == 2U && strcmp(capture.sent[1], "retry") == 0);

    USART1TxScheduler_Fail(&scheduler);
    assert(!scheduler.busy && scheduler.active_slot == -1);
    assert(scheduler.failed_transmissions == 1U);
    assert(!USART1TxScheduler_Process(&scheduler, capture_start, &capture));
    assert(USART1TxScheduler_Enqueue(&scheduler, USART1_TX_LOG, "next", 4U) == USART1_TX_OK);
    assert(USART1TxScheduler_Process(&scheduler, capture_start, &capture));
    USART1TxScheduler_Init(&scheduler);
    assert(USART1TxScheduler_Enqueue(&scheduler, USART1_TX_LOG, "fast", 4U) == USART1_TX_OK);
    assert(USART1TxScheduler_Process(&scheduler, complete_during_start, &scheduler));
    assert(!scheduler.busy && scheduler.active_slot == -1);
    return 0;
}
