#include "wifi_state.h"

#include <assert.h>

int main(void)
{
    WiFiState state;

    WiFiState_Init(&state);
    assert(state.status == WIFI_STATUS_NO_CREDENTIALS);
    assert(state.credentials_loaded == 0U);

    WiFiState_BeginSync(&state);
    assert(state.status == WIFI_STATUS_MODULE_SYNCING);
    WiFiState_SetUnknown(&state);
    assert(state.status == WIFI_STATUS_UNKNOWN);
    assert(!WiFiState_RequestConnect(&state, false, true));
    assert(state.status == WIFI_STATUS_UNKNOWN);

    assert(WiFiState_RequestConnect(&state, true, true));
    assert(state.status == WIFI_STATUS_CONNECTING);
    assert(state.credentials_loaded == 1U);
    WiFiState_ConnectSucceeded(&state);
    assert(state.status == WIFI_STATUS_CONNECTED);

    WiFiState_Init(&state);
    WiFiState_BeginSync(&state);
    WiFiState_AdoptConnected(&state);
    assert(state.status == WIFI_STATUS_CONNECTED);
    assert(state.credentials_loaded == 0U);
    WiFiState_ConnectionLost(&state, 100U);
    assert(state.status == WIFI_STATUS_NO_CREDENTIALS);
    assert(!WiFiState_RetryDue(&state, 100000U));

    assert(WiFiState_RequestConnect(&state, true, true));
    WiFiState_ConnectFailed(&state, 1000U);
    assert(state.status == WIFI_STATUS_RETRY_WAIT);
    assert(state.retry_deadline == 6000U);
    assert(!WiFiState_RetryDue(&state, 5999U));
    assert(WiFiState_RetryDue(&state, 6000U));
    assert(state.status == WIFI_STATUS_CONNECTING);
    WiFiState_ConnectFailed(&state, 6000U);
    assert(state.retry_deadline == 21000U);
    assert(WiFiState_RetryDue(&state, 21000U));
    WiFiState_ConnectFailed(&state, 21000U);
    assert(state.retry_deadline == 51000U);
    assert(WiFiState_RetryDue(&state, 51000U));
    WiFiState_ConnectFailed(&state, 51000U);
    assert(state.retry_deadline == 81000U);

    WiFiState_Clear(&state);
    assert(state.status == WIFI_STATUS_NO_CREDENTIALS);
    assert(state.credentials_loaded == 0U);
    assert(!WiFiState_RetryDue(&state, 100000U));
    return 0;
}
