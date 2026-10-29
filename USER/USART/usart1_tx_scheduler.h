#ifndef USART1_TX_SCHEDULER_H
#define USART1_TX_SCHEDULER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define USART1_TX_MESSAGE_CAPACITY 192U
#define USART1_TX_REPLY_CAPACITY 4U
#define USART1_TX_LOG_CAPACITY 12U
#define USART1_TX_SLOT_COUNT (USART1_TX_REPLY_CAPACITY + USART1_TX_LOG_CAPACITY)

typedef enum {
    USART1_TX_LOG = 0,
    USART1_TX_REPLY = 1
} USART1TxPriority;

typedef enum {
    USART1_TX_OK = 0,
    USART1_TX_FULL,
    USART1_TX_INVALID
} USART1TxResult;

typedef struct {
    uint8_t bytes[USART1_TX_MESSAGE_CAPACITY];
    uint16_t length;
    uint32_t sequence;
    uint8_t priority;
    uint8_t occupied;
} USART1TxSlot;

typedef struct {
    USART1TxSlot slots[USART1_TX_SLOT_COUNT];
    uint32_t next_sequence;
    uint32_t dropped_logs;
    uint32_t rejected_replies;
    uint32_t failed_transmissions;
    int8_t active_slot;
    uint8_t busy;
} USART1TxScheduler;

typedef int (*USART1TxStartFn)(void *context, const uint8_t *data, size_t length);

void USART1TxScheduler_Init(USART1TxScheduler *scheduler);
USART1TxResult USART1TxScheduler_Enqueue(USART1TxScheduler *scheduler,
                                         USART1TxPriority priority,
                                         const void *data, size_t length);
bool USART1TxScheduler_Process(USART1TxScheduler *scheduler,
                              USART1TxStartFn start, void *context);
void USART1TxScheduler_Complete(USART1TxScheduler *scheduler);
void USART1TxScheduler_Fail(USART1TxScheduler *scheduler);

#endif
