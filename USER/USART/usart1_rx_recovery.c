#include "usart1_rx_recovery.h"

void USART1RxRecovery_Init(USART1RxRecovery *recovery)
{
    if (recovery != 0) {
        recovery->discard_until_eol = 0U;
        recovery->events = 0U;
    }
}

bool USART1RxRecovery_HandleError(USART1RxRecovery *recovery, bool is_usart1,
                                 uint32_t error_flags)
{
    if (recovery == 0 || !is_usart1 || (error_flags & USART1_RX_ERROR_ORE) == 0U) {
        return false;
    }
    recovery->discard_until_eol = 1U;
    recovery->events |= USART1_RX_EVENT_RECOVERED;
    return true;
}

bool USART1RxRecovery_ShouldDiscard(USART1RxRecovery *recovery, uint8_t byte)
{
    if (recovery == 0 || recovery->discard_until_eol == 0U) {
        return false;
    }
    if (byte == '\r' || byte == '\n') {
        recovery->discard_until_eol = 0U;
    }
    return true;
}

void USART1RxRecovery_ReportRearmFailure(USART1RxRecovery *recovery)
{
    if (recovery != 0) {
        recovery->events |= USART1_RX_EVENT_REARM_FAILED;
    }
}

uint8_t USART1RxRecovery_TakeEvents(USART1RxRecovery *recovery)
{
    uint8_t events;

    if (recovery == 0) {
        return 0U;
    }
    events = recovery->events;
    recovery->events = 0U;
    return events;
}
