#include "usart1_tx_scheduler.h"

#include <string.h>

void USART1TxScheduler_Init(USART1TxScheduler *scheduler)
{
    if (scheduler == NULL) {
        return;
    }
    memset(scheduler, 0, sizeof(*scheduler));
    scheduler->active_slot = -1;
}

USART1TxResult USART1TxScheduler_Enqueue(USART1TxScheduler *scheduler,
                                         USART1TxPriority priority,
                                         const void *data, size_t length)
{
    size_t i;
    size_t matching_count = 0U;
    int free_slot = -1;

    if (scheduler == NULL || data == NULL || length == 0U ||
        length >= USART1_TX_MESSAGE_CAPACITY ||
        (priority != USART1_TX_LOG && priority != USART1_TX_REPLY)) {
        return USART1_TX_INVALID;
    }
    for (i = 0U; i < USART1_TX_SLOT_COUNT; i++) {
        if (scheduler->slots[i].occupied != 0U && scheduler->slots[i].priority == priority) {
            matching_count++;
        } else if (scheduler->slots[i].occupied == 0U && free_slot < 0) {
            free_slot = (int)i;
        }
    }
    if ((priority == USART1_TX_REPLY && matching_count >= USART1_TX_REPLY_CAPACITY) ||
        (priority == USART1_TX_LOG && matching_count >= USART1_TX_LOG_CAPACITY) ||
        free_slot < 0) {
        if (priority == USART1_TX_REPLY) {
            scheduler->rejected_replies++;
        } else {
            scheduler->dropped_logs++;
        }
        return USART1_TX_FULL;
    }

    USART1TxSlot *slot = &scheduler->slots[free_slot];
    memcpy(slot->bytes, data, length);
    slot->length = (uint16_t)length;
    slot->sequence = scheduler->next_sequence++;
    slot->priority = (uint8_t)priority;
    slot->occupied = 1U;
    return USART1_TX_OK;
}

bool USART1TxScheduler_Process(USART1TxScheduler *scheduler,
                              USART1TxStartFn start, void *context)
{
    int selected = -1;
    size_t i;
    USART1TxSlot *slot;

    if (scheduler == NULL || start == NULL || scheduler->busy != 0U) {
        return false;
    }
    for (i = 0U; i < USART1_TX_SLOT_COUNT; i++) {
        if (scheduler->slots[i].occupied == 0U) {
            continue;
        }
        if (selected < 0 || scheduler->slots[i].priority > scheduler->slots[selected].priority ||
            (scheduler->slots[i].priority == scheduler->slots[selected].priority &&
             (int32_t)(scheduler->slots[i].sequence - scheduler->slots[selected].sequence) < 0)) {
            selected = (int)i;
        }
    }
    if (selected < 0) {
        return false;
    }

    slot = &scheduler->slots[selected];
    scheduler->active_slot = (int8_t)selected;
    scheduler->busy = 1U;
    if (start(context, slot->bytes, slot->length) == 0) {
        scheduler->active_slot = -1;
        scheduler->busy = 0U;
        return false;
    }
    return true;
}

void USART1TxScheduler_Complete(USART1TxScheduler *scheduler)
{
    if (scheduler == NULL || scheduler->active_slot < 0 ||
        scheduler->active_slot >= (int8_t)USART1_TX_SLOT_COUNT) {
        return;
    }
    scheduler->slots[(uint8_t)scheduler->active_slot].occupied = 0U;
    scheduler->slots[(uint8_t)scheduler->active_slot].length = 0U;
    scheduler->active_slot = -1;
    scheduler->busy = 0U;
}

void USART1TxScheduler_Fail(USART1TxScheduler *scheduler)
{
    USART1TxSlot *slot;

    if (scheduler == NULL || scheduler->active_slot < 0 ||
        scheduler->active_slot >= (int8_t)USART1_TX_SLOT_COUNT) {
        return;
    }
    slot = &scheduler->slots[(uint8_t)scheduler->active_slot];
    scheduler->failed_transmissions++;
    slot->occupied = 0U;
    slot->length = 0U;
    scheduler->active_slot = -1;
    scheduler->busy = 0U;
}
