#include "wifi_state.h"

#include <string.h>

static uint32_t RetryDelayMilliseconds(uint8_t retry_count)
{
    if (retry_count == 0U) {
        return 5000U;
    }
    if (retry_count == 1U) {
        return 15000U;
    }
    return 30000U;
}

static void ScheduleRetry(WiFiState *state, uint32_t now)
{
    if (state->credentials_loaded == 0U || state->retry_enabled == 0U) {
        state->status = WIFI_STATUS_NO_CREDENTIALS;
        return;
    }
    state->retry_deadline = now + RetryDelayMilliseconds(state->retry_count);
    if (state->retry_count < 3U) {
        state->retry_count++;
    }
    state->status = WIFI_STATUS_RETRY_WAIT;
}

void WiFiState_Init(WiFiState *state)
{
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
    state->status = WIFI_STATUS_NO_CREDENTIALS;
}

void WiFiState_BeginSync(WiFiState *state)
{
    if (state != NULL) {
        state->status = WIFI_STATUS_MODULE_SYNCING;
    }
}

void WiFiState_SetUnknown(WiFiState *state)
{
    if (state != NULL) {
        state->status = WIFI_STATUS_UNKNOWN;
    }
}

void WiFiState_AdoptConnected(WiFiState *state)
{
    if (state != NULL) {
        state->status = WIFI_STATUS_CONNECTED;
        state->retry_count = 0U;
        state->retry_enabled = state->credentials_loaded;
    }
}

bool WiFiState_RequestConnect(WiFiState *state, bool dialect_known, bool credentials_valid)
{
    if (state == NULL || !dialect_known || !credentials_valid) {
        return false;
    }
    state->credentials_loaded = 1U;
    state->retry_enabled = 1U;
    state->retry_count = 0U;
    state->status = WIFI_STATUS_CONNECTING;
    return true;
}

void WiFiState_ConnectSucceeded(WiFiState *state)
{
    WiFiState_AdoptConnected(state);
}

void WiFiState_ConnectFailed(WiFiState *state, uint32_t now)
{
    if (state != NULL) {
        ScheduleRetry(state, now);
    }
}

void WiFiState_ConnectionLost(WiFiState *state, uint32_t now)
{
    if (state == NULL) {
        return;
    }
    state->status = WIFI_STATUS_UPSTREAM_LOST;
    ScheduleRetry(state, now);
}

bool WiFiState_RetryDue(WiFiState *state, uint32_t now)
{
    if (state == NULL || state->status != WIFI_STATUS_RETRY_WAIT ||
        (int32_t)(now - state->retry_deadline) < 0) {
        return false;
    }
    state->status = WIFI_STATUS_CONNECTING;
    return true;
}

void WiFiState_Clear(WiFiState *state)
{
    WiFiState_Init(state);
}
