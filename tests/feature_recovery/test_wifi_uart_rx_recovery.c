#include "wifi_uart_rx_recovery.h"

#include <assert.h>

static bool RearmSucceeds(void *context)
{
    unsigned int *calls = (unsigned int *)context;
    (*calls)++;
    return true;
}

static bool RearmFails(void *context)
{
    unsigned int *calls = (unsigned int *)context;
    (*calls)++;
    return false;
}

int main(void)
{
    WiFiUartRxRecovery recovery = {0};
    unsigned int calls = 0U;

    assert(!wifi_uart_rx_recovery_defer_rearm_on_rx_complete(0U, 0x08U));
    assert(wifi_uart_rx_recovery_defer_rearm_on_rx_complete(0x08U, 0x08U));
    assert(wifi_uart_rx_recovery_handle_error(0x08U, 0U, RearmSucceeds, &calls) ==
           WIFI_UART_RX_RECOVERY_NOT_REQUIRED);
    assert(calls == 0U);
    assert(wifi_uart_rx_recovery_handle_error(0x08U, 0x08U, RearmSucceeds, &calls) ==
           WIFI_UART_RX_RECOVERY_REARMED);
    assert(calls == 1U);
    assert(wifi_uart_rx_recovery_handle_error_after_rx(0x08U, 0U, true,
                                                       RearmSucceeds, &calls) ==
           WIFI_UART_RX_RECOVERY_REARMED);
    assert(calls == 2U);
    assert(wifi_uart_rx_recovery_handle_error_after_rx(0x08U, 0x08U, false,
                                                       RearmFails, &calls) ==
           WIFI_UART_RX_RECOVERY_REARM_FAILED);
    assert(calls == 3U);

    wifi_uart_rx_recovery_discard_partial_line(&recovery);
    assert(wifi_uart_rx_recovery_should_discard(&recovery, 'x'));
    assert(wifi_uart_rx_recovery_should_discard(&recovery, '\r'));
    assert(!wifi_uart_rx_recovery_should_discard(&recovery, 'O'));
    return 0;
}
