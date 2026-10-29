#include "iot.h"

#include "bluetooth.h"
#include "board_io.h"
#include "usart.h"
#include "wifi.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static IoT_Status g_iot_status;
static uint32_t g_last_uptime_tick;
static BluetoothCommandParser g_bt_parser;

#ifndef IOT_AUTO_TCP_DIAGNOSTIC_ENABLE
#define IOT_AUTO_TCP_DIAGNOSTIC_ENABLE 0
#endif

#if IOT_AUTO_TCP_DIAGNOSTIC_ENABLE
#define IOT_AUTO_TCP_RETRY_INITIAL_MS 1000U
#define IOT_AUTO_TCP_RETRY_MAX_MS 30000U

typedef enum {
    IOT_AUTO_TCP_WIFI_STATUS_QUERY_REQUEST = 0,
    IOT_AUTO_TCP_WIFI_STATUS_QUERY_PENDING,
    IOT_AUTO_TCP_TCP_STATUS_QUERY_REQUEST,
    IOT_AUTO_TCP_TCP_STATUS_QUERY_PENDING,
    IOT_AUTO_TCP_CONNECT_PENDING,
    IOT_AUTO_TCP_SEND_PENDING,
    IOT_AUTO_TCP_CLOSE_PENDING,
    IOT_AUTO_TCP_RECOVERY_CLOSE_PENDING,
    IOT_AUTO_TCP_DONE
} IoT_AutoTcpPhase;

/* Enable only when the one-shot host receiver is running on the hotspot. */
static const char g_auto_tcp_host[] = "192.168.137.1";
static const uint16_t g_auto_tcp_port = 43117U;
static const uint8_t g_auto_tcp_payload[] = "PING\r\n";
static IoT_AutoTcpPhase g_auto_tcp_phase;
static uint32_t g_auto_tcp_next_action_tick;
static uint32_t g_auto_tcp_retry_delay_ms;

static bool AutoTcpDeadlineReached(uint32_t now)
{
    return (int32_t)(now - g_auto_tcp_next_action_tick) >= 0;
}

static void ScheduleAutoTcpRetry(uint32_t now)
{
    g_auto_tcp_next_action_tick = now + g_auto_tcp_retry_delay_ms;
    if (g_auto_tcp_retry_delay_ms < IOT_AUTO_TCP_RETRY_MAX_MS / 2U) {
        g_auto_tcp_retry_delay_ms *= 2U;
    } else {
        g_auto_tcp_retry_delay_ms = IOT_AUTO_TCP_RETRY_MAX_MS;
    }
}
#endif

static void SetLedState(uint8_t index, bool enabled)
{
    LED_Set(index, enabled);
    g_iot_status.led_state[index] = enabled ? 1U : 0U;
}

static void SetBuzzerState(bool enabled)
{
    BEEP_Set(enabled);
    g_iot_status.buzzer_state = enabled ? 1U : 0U;
}

static void BluetoothRxByte(uint8_t byte)
{
    BluetoothCommand command;
    if (BluetoothCommandParser_Feed(&g_bt_parser, byte, &command)) {
        (void)IoT_HandleBluetoothCommand(command);
    }
}

#if IOT_AUTO_TCP_DIAGNOSTIC_ENABLE
static void ProcessAutoTcpCommand(void)
{
    WiFiTcpOperationResult result;
    uint32_t now = HAL_GetTick();

    if (g_auto_tcp_phase == IOT_AUTO_TCP_WIFI_STATUS_QUERY_REQUEST) {
        if (!AutoTcpDeadlineReached(now)) {
            return;
        }
        if (WiFi_RequestStatusCheck()) {
            g_auto_tcp_phase = IOT_AUTO_TCP_WIFI_STATUS_QUERY_PENDING;
        } else {
            ScheduleAutoTcpRetry(now);
        }
        return;
    }

    if (g_auto_tcp_phase == IOT_AUTO_TCP_WIFI_STATUS_QUERY_PENDING) {
        WiFiRuntimeInfo runtime;
        WiFiStatus status;

        if (!WiFi_TakeStatusCheckResult(&status)) {
            return;
        }
        if (status != WIFI_STATUS_CONNECTED || !WiFi_IsConnected() ||
            !WiFi_GetRuntimeInfo(&runtime) ||
            runtime.station != WIFI_AT_STATION_CONNECTED ||
            runtime.ip != WIFI_IP_READY) {
            ScheduleAutoTcpRetry(now);
            g_auto_tcp_phase = IOT_AUTO_TCP_WIFI_STATUS_QUERY_REQUEST;
            return;
        }
        g_auto_tcp_phase = IOT_AUTO_TCP_TCP_STATUS_QUERY_REQUEST;
        g_auto_tcp_next_action_tick = now;
        return;
    }

    if (g_auto_tcp_phase == IOT_AUTO_TCP_TCP_STATUS_QUERY_REQUEST) {
        if (!AutoTcpDeadlineReached(now)) {
            return;
        }
        if (WiFi_RequestTcpStatusCheck()) {
            g_auto_tcp_phase = IOT_AUTO_TCP_TCP_STATUS_QUERY_PENDING;
        } else {
            ScheduleAutoTcpRetry(now);
            g_auto_tcp_phase = IOT_AUTO_TCP_WIFI_STATUS_QUERY_REQUEST;
        }
        return;
    }

    if (g_auto_tcp_phase == IOT_AUTO_TCP_TCP_STATUS_QUERY_PENDING) {
        WiFiAtTcpStatus tcp_status;
        WiFiRuntimeInfo runtime;

        if (!WiFi_TakeTcpStatusCheckResult(&tcp_status)) {
            return;
        }
        if (tcp_status == WIFI_AT_TCP_DISCONNECTED) {
            if (WiFi_GetStatus() != WIFI_STATUS_CONNECTED ||
                !WiFi_IsConnected() || !WiFi_GetRuntimeInfo(&runtime) ||
                runtime.station != WIFI_AT_STATION_CONNECTED ||
                runtime.ip != WIFI_IP_READY) {
                ScheduleAutoTcpRetry(now);
                g_auto_tcp_phase = IOT_AUTO_TCP_WIFI_STATUS_QUERY_REQUEST;
                return;
            }
            if (WiFi_TcpConnect(g_auto_tcp_host, g_auto_tcp_port)) {
                g_auto_tcp_phase = IOT_AUTO_TCP_CONNECT_PENDING;
            } else {
                ScheduleAutoTcpRetry(now);
                g_auto_tcp_phase = IOT_AUTO_TCP_WIFI_STATUS_QUERY_REQUEST;
            }
        } else if (tcp_status == WIFI_AT_TCP_UNKNOWN) {
            ScheduleAutoTcpRetry(now);
            g_auto_tcp_phase = IOT_AUTO_TCP_WIFI_STATUS_QUERY_REQUEST;
        } else {
            /* A socket exists, but no successful connect result was received.
             * Close it before retrying; never send the one-shot command here. */
            if (WiFi_TcpClose()) {
                g_auto_tcp_phase = IOT_AUTO_TCP_RECOVERY_CLOSE_PENDING;
            } else {
                ScheduleAutoTcpRetry(now);
                g_auto_tcp_phase = IOT_AUTO_TCP_WIFI_STATUS_QUERY_REQUEST;
            }
        }
        return;
    }

    if (g_auto_tcp_phase == IOT_AUTO_TCP_CONNECT_PENDING) {
        if (!WiFi_TakeTcpOperationResult(&result)) {
            return;
        }
        if (result != WIFI_TCP_OPERATION_SUCCEEDED) {
            /* Refresh STA/IP before every retry: a cached connected state can
             * survive an ESP disconnect and repeatedly trigger rejected
             * CIPSTART commands. */
            ScheduleAutoTcpRetry(now);
            g_auto_tcp_phase = IOT_AUTO_TCP_WIFI_STATUS_QUERY_REQUEST;
            return;
        }
        if (WiFi_TcpSend(g_auto_tcp_payload, sizeof(g_auto_tcp_payload) - 1U)) {
            g_auto_tcp_phase = IOT_AUTO_TCP_SEND_PENDING;
        } else if (WiFi_TcpClose()) {
            g_auto_tcp_phase = IOT_AUTO_TCP_CLOSE_PENDING;
        } else {
            g_auto_tcp_phase = IOT_AUTO_TCP_DONE;
        }
        return;
    }

    if (g_auto_tcp_phase == IOT_AUTO_TCP_SEND_PENDING) {
        if (!WiFi_TakeTcpOperationResult(&result)) {
            return;
        }
        if (WiFi_TcpClose()) {
            g_auto_tcp_phase = IOT_AUTO_TCP_CLOSE_PENDING;
        } else {
            g_auto_tcp_phase = IOT_AUTO_TCP_DONE;
        }
        return;
    }

    if (g_auto_tcp_phase == IOT_AUTO_TCP_CLOSE_PENDING &&
        WiFi_TakeTcpOperationResult(&result)) {
        g_auto_tcp_phase = IOT_AUTO_TCP_DONE;
        return;
    }

    if (g_auto_tcp_phase == IOT_AUTO_TCP_RECOVERY_CLOSE_PENDING &&
        WiFi_TakeTcpOperationResult(&result)) {
        ScheduleAutoTcpRetry(now);
        g_auto_tcp_phase = IOT_AUTO_TCP_WIFI_STATUS_QUERY_REQUEST;
    }
}
#endif

static IoT_Result HandleLed(const IoT_Command *command)
{
    uint8_t index;
    bool enabled;
    char response[48];

    if (command->param > 2U) {
        return IOT_RESULT_INVALID_PARAM;
    }
    index = (uint8_t)command->param;
    if (command->action == IOT_ACTION_SET) {
        enabled = true;
    } else if (command->action == IOT_ACTION_GET) {
        enabled = LED_Get(index);
    } else {
        enabled = !LED_Get(index);
    }
    if (command->action != IOT_ACTION_GET) {
        SetLedState(index, enabled);
    } else {
        g_iot_status.led_state[index] = enabled ? 1U : 0U;
    }
    (void)snprintf(response, sizeof(response), "LED[%u]=%s\r\n",
                   (unsigned int)index, enabled ? "ON" : "OFF");
    IoT_SendResponse(response);
    return IOT_RESULT_OK;
}

static IoT_Result HandleBuzzer(const IoT_Command *command)
{
    bool enabled;
    if (command->action == IOT_ACTION_SET) {
        enabled = command->param != 0U;
    } else if (command->action == IOT_ACTION_GET) {
        enabled = BEEP_Get();
    } else {
        enabled = !BEEP_Get();
    }
    if (command->action != IOT_ACTION_GET) {
        SetBuzzerState(enabled);
    } else {
        g_iot_status.buzzer_state = enabled ? 1U : 0U;
    }
    IoT_SendResponse(enabled ? "BUZZER=ON\r\n" : "BUZZER=OFF\r\n");
    return IOT_RESULT_OK;
}

static IoT_Result HandleSystem(const IoT_Command *command)
{
    char response[80];
    if (command->action != IOT_ACTION_GET) {
        return IOT_RESULT_INVALID_CMD;
    }
    (void)snprintf(response, sizeof(response), "SYSTEM WIFI=%u BT=%u UPTIME=%lu\r\n",
                   (unsigned int)g_iot_status.wifi_connected,
                   (unsigned int)g_iot_status.bt_connected,
                   (unsigned long)g_iot_status.uptime);
    IoT_SendResponse(response);
    return IOT_RESULT_OK;
}

void IoT_Init(const char *device_id)
{
    memset(&g_iot_status, 0, sizeof(g_iot_status));
    if (device_id != NULL) {
        strncpy(g_iot_status.device_id, device_id, sizeof(g_iot_status.device_id) - 1U);
        g_iot_status.device_id[sizeof(g_iot_status.device_id) - 1U] = '\0';
    }
    g_last_uptime_tick = HAL_GetTick();
    BluetoothCommandParser_Init(&g_bt_parser);
#if IOT_AUTO_TCP_DIAGNOSTIC_ENABLE
    g_auto_tcp_phase = IOT_AUTO_TCP_WIFI_STATUS_QUERY_REQUEST;
    g_auto_tcp_next_action_tick = HAL_GetTick();
    g_auto_tcp_retry_delay_ms = IOT_AUTO_TCP_RETRY_INITIAL_MS;
#endif
    BT_Init();
    BT_SetRxCallback(BluetoothRxByte);
    WiFi_Init();
}

void IoT_Process(void)
{
    uint32_t now = HAL_GetTick();
    if ((uint32_t)(now - g_last_uptime_tick) >= 1000U) {
        g_iot_status.uptime++;
        g_last_uptime_tick = now;
    }
    WiFi_Process();
#if IOT_AUTO_TCP_DIAGNOSTIC_ENABLE
    ProcessAutoTcpCommand();
#endif
    BT_Process();
    g_iot_status.wifi_connected = WiFi_IsConnected() ? 1U : 0U;
    g_iot_status.bt_connected = BT_IsConnected() ? 1U : 0U;
}

IoT_Status *IoT_GetStatus(void)
{
    return &g_iot_status;
}

IoT_Result IoT_HandleCommand(const IoT_Command *command)
{
    if (command == NULL) {
        return IOT_RESULT_INVALID_PARAM;
    }
    if (command->module >= IOT_MODULE_MAX || command->action >= IOT_ACTION_MAX) {
        return IOT_RESULT_INVALID_CMD;
    }
    if (command->module == IOT_MODULE_LED) {
        return HandleLed(command);
    }
    if (command->module == IOT_MODULE_BUZZER) {
        return HandleBuzzer(command);
    }
    return HandleSystem(command);
}

IoT_Result IoT_HandleBluetoothCommand(BluetoothCommand command)
{
    switch (command) {
    case BT_COMMAND_LED1_ON: SetLedState(0U, true); break;
    case BT_COMMAND_LED1_OFF: SetLedState(0U, false); break;
    case BT_COMMAND_LED2_ON: SetLedState(1U, true); break;
    case BT_COMMAND_LED2_OFF: SetLedState(1U, false); break;
    case BT_COMMAND_LED3_ON: SetLedState(2U, true); break;
    case BT_COMMAND_LED3_OFF: SetLedState(2U, false); break;
    case BT_COMMAND_BUZZER_ON: SetBuzzerState(true); break;
    case BT_COMMAND_BUZZER_OFF: SetBuzzerState(false); break;
    default: return IOT_RESULT_INVALID_CMD;
    }
    return IOT_RESULT_OK;
}

void IoT_SendResponse(const char *response)
{
    USART1_SendReply(response);
}
