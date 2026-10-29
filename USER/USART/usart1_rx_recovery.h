#ifndef USART1_RX_RECOVERY_H
#define USART1_RX_RECOVERY_H

#include <stdbool.h>
#include <stdint.h>

#define USART1_RX_ERROR_NONE 0U
#define USART1_RX_ERROR_ORE 0x08U
#define USART1_RX_EVENT_REARM_FAILED 0x01U
#define USART1_RX_EVENT_RECOVERED 0x02U

typedef struct {
    volatile uint8_t discard_until_eol;
    volatile uint8_t events;
} USART1RxRecovery;

void USART1RxRecovery_Init(USART1RxRecovery *recovery);
bool USART1RxRecovery_HandleError(USART1RxRecovery *recovery, bool is_usart1,
                                 uint32_t error_flags);
bool USART1RxRecovery_ShouldDiscard(USART1RxRecovery *recovery, uint8_t byte);
void USART1RxRecovery_ReportRearmFailure(USART1RxRecovery *recovery);
uint8_t USART1RxRecovery_TakeEvents(USART1RxRecovery *recovery);

#endif
