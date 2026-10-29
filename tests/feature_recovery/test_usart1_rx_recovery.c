#include "usart1_rx_recovery.h"

#include <assert.h>

int main(void)
{
    USART1RxRecovery recovery;

    USART1RxRecovery_Init(&recovery);
    assert(!USART1RxRecovery_HandleError(&recovery, 0, USART1_RX_ERROR_ORE));
    assert(!recovery.discard_until_eol);
    assert(!USART1RxRecovery_HandleError(&recovery, 1, USART1_RX_ERROR_NONE));
    assert(!recovery.discard_until_eol);

    assert(USART1RxRecovery_HandleError(&recovery, 1, USART1_RX_ERROR_ORE));
    assert(recovery.discard_until_eol);
    assert(USART1RxRecovery_ShouldDiscard(&recovery, 'W'));
    assert(USART1RxRecovery_ShouldDiscard(&recovery, '\r'));
    assert(!recovery.discard_until_eol);
    assert(!USART1RxRecovery_ShouldDiscard(&recovery, 'S'));

    assert(USART1RxRecovery_HandleError(&recovery, 1, USART1_RX_ERROR_ORE));
    USART1RxRecovery_ReportRearmFailure(&recovery);
    assert((USART1RxRecovery_TakeEvents(&recovery) & USART1_RX_EVENT_REARM_FAILED) != 0U);
    assert(USART1RxRecovery_TakeEvents(&recovery) == 0U);
    return 0;
}
