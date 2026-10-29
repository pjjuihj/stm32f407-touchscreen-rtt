#ifndef WIFI_UART_RX_RECOVERY_H
#define WIFI_UART_RX_RECOVERY_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    WIFI_UART_RX_RECOVERY_NOT_REQUIRED = 0,
    WIFI_UART_RX_RECOVERY_REARMED = 1,
    WIFI_UART_RX_RECOVERY_REARM_FAILED = 2
} WiFiUartRxRecoveryResult;

typedef struct {
    volatile uint8_t discard_until_eol;
} WiFiUartRxRecovery;

typedef bool (*WiFiUartRxRearmFn)(void *context);

bool wifi_uart_rx_recovery_defer_rearm_on_rx_complete(uint32_t error_code,
                                                       uint32_t receive_error_mask);
WiFiUartRxRecoveryResult wifi_uart_rx_recovery_handle_error(
    uint32_t recoverable_error_mask, uint32_t error_code,
    WiFiUartRxRearmFn rearm, void *context);
WiFiUartRxRecoveryResult wifi_uart_rx_recovery_handle_error_after_rx(
    uint32_t recoverable_error_mask, uint32_t error_code, bool rearm_was_deferred,
    WiFiUartRxRearmFn rearm, void *context);
void wifi_uart_rx_recovery_discard_partial_line(WiFiUartRxRecovery *recovery);
bool wifi_uart_rx_recovery_should_discard(WiFiUartRxRecovery *recovery, uint8_t byte);

#endif
