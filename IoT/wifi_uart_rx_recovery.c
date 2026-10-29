#include "wifi_uart_rx_recovery.h"

#include <stddef.h>

bool wifi_uart_rx_recovery_defer_rearm_on_rx_complete(uint32_t error_code,
                                                       uint32_t receive_error_mask)
{
    return (error_code & receive_error_mask) != 0U;
}

WiFiUartRxRecoveryResult wifi_uart_rx_recovery_handle_error_after_rx(
    uint32_t recoverable_error_mask, uint32_t error_code, bool rearm_was_deferred,
    WiFiUartRxRearmFn rearm, void *context)
{
    if (recoverable_error_mask == 0U ||
        (((error_code & recoverable_error_mask) == 0U) && !rearm_was_deferred)) {
        return WIFI_UART_RX_RECOVERY_NOT_REQUIRED;
    }
    if (rearm == NULL || !rearm(context)) {
        return WIFI_UART_RX_RECOVERY_REARM_FAILED;
    }
    return WIFI_UART_RX_RECOVERY_REARMED;
}

WiFiUartRxRecoveryResult wifi_uart_rx_recovery_handle_error(
    uint32_t recoverable_error_mask, uint32_t error_code,
    WiFiUartRxRearmFn rearm, void *context)
{
    return wifi_uart_rx_recovery_handle_error_after_rx(
        recoverable_error_mask, error_code, false, rearm, context);
}

void wifi_uart_rx_recovery_discard_partial_line(WiFiUartRxRecovery *recovery)
{
    if (recovery != NULL) {
        recovery->discard_until_eol = 1U;
    }
}

bool wifi_uart_rx_recovery_should_discard(WiFiUartRxRecovery *recovery, uint8_t byte)
{
    if (recovery == NULL || recovery->discard_until_eol == 0U) {
        return false;
    }
    if (byte == '\r' || byte == '\n') {
        recovery->discard_until_eol = 0U;
    }
    return true;
}
