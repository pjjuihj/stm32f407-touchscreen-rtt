#ifndef WIFI_STATE_H
#define WIFI_STATE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    WIFI_STATUS_NO_CREDENTIALS = 0,
    WIFI_STATUS_MODULE_SYNCING,
    WIFI_STATUS_UNKNOWN,
    WIFI_STATUS_CONNECTING,
    WIFI_STATUS_CONNECTED,
    WIFI_STATUS_RETRY_WAIT,
    WIFI_STATUS_UPSTREAM_LOST
} WiFiStatus;

typedef struct {
    WiFiStatus status;
    uint8_t credentials_loaded;
    uint8_t retry_count;
    uint8_t retry_enabled;
    uint32_t retry_deadline;
} WiFiState;

void WiFiState_Init(WiFiState *state);
void WiFiState_BeginSync(WiFiState *state);
void WiFiState_SetUnknown(WiFiState *state);
void WiFiState_AdoptConnected(WiFiState *state);
bool WiFiState_RequestConnect(WiFiState *state, bool dialect_known, bool credentials_valid);
void WiFiState_ConnectSucceeded(WiFiState *state);
void WiFiState_ConnectFailed(WiFiState *state, uint32_t now);
void WiFiState_ConnectionLost(WiFiState *state, uint32_t now);
bool WiFiState_RetryDue(WiFiState *state, uint32_t now);
void WiFiState_Clear(WiFiState *state);

#endif
