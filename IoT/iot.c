#include "iot.h"

#include "bluetooth.h"
#include "board_io.h"
#include "usart.h"
#include "wifi.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#ifndef IOT_MQTT_ENABLE
#define IOT_MQTT_ENABLE 1
#endif

#if IOT_MQTT_ENABLE
#include "mqtt_client.h"
#include "stm32f4xx_hal_rng.h"
#endif

static IoT_Status g_iot_status;
static uint32_t g_last_uptime_tick;
static BluetoothCommandParser g_bt_parser;
#if IOT_MQTT_ENABLE
static RNG_HandleTypeDef g_mqtt_rng;
static uint8_t g_mqtt_rng_ready;
static uint8_t g_have_last_buzzer_pulse;
static uint32_t g_buzzer_pulse_off_tick;
static uint32_t g_last_buzzer_pulse_tick;
#endif
static uint8_t g_buzzer_pulse_active;

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
    g_buzzer_pulse_active = 0U;
    BEEP_Set(enabled);
    g_iot_status.buzzer_state = enabled ? 1U : 0U;
}

#if IOT_MQTT_ENABLE
static bool GenerateMqttSessionId(char output[MQTT_JSON_ID_LENGTH + 1U])
{
    uint32_t high;
    uint32_t low;
    int length;

    if (output == NULL) {
        return false;
    }
    output[0] = '\0';
    memset(&g_mqtt_rng, 0, sizeof(g_mqtt_rng));
    g_mqtt_rng.Instance = RNG;
    __HAL_RCC_RNG_CLK_ENABLE();
    if (HAL_RNG_Init(&g_mqtt_rng) != HAL_OK ||
        HAL_RNG_GenerateRandomNumber(&g_mqtt_rng, &high) != HAL_OK ||
        HAL_RNG_GenerateRandomNumber(&g_mqtt_rng, &low) != HAL_OK) {
        g_mqtt_rng_ready = 0U;
        return false;
    }
    length = snprintf(output, MQTT_JSON_ID_LENGTH + 1U, "%08lx%08lx",
                      (unsigned long)high, (unsigned long)low);
    if (length != (int)MQTT_JSON_ID_LENGTH) {
        output[0] = '\0';
        g_mqtt_rng_ready = 0U;
        return false;
    }
    g_mqtt_rng_ready = 1U;
    return true;
}

static void ReadMqttActuators(MqttActuatorReadback *readback, void *context)
{
    size_t index;
    (void)context;
    if (readback == NULL) {
        return;
    }
    for (index = 0U; index < 3U; index++) {
        readback->led[index] = LED_Get((uint8_t)index) ? 1U : 0U;
    }
    readback->buzzer_gpio_high = BEEP_Get() ? 1U : 0U;
}

static const char *HandleMqttCommand(const MqttCommand *command,
                                     MqttActuatorReadback *readback,
                                     void *context)
{
    uint32_t now = HAL_GetTick();
    (void)context;

    if (command == NULL) {
        ReadMqttActuators(readback, NULL);
        return "INVALID";
    }
    if (command->type == MQTT_COMMAND_LED_SET) {
        if (command->led_index > 2U) {
            ReadMqttActuators(readback, NULL);
            return "INVALID_PARAM";
        }
        SetLedState(command->led_index, command->led_on != 0U);
    } else if (command->type == MQTT_COMMAND_BUZZER_PULSE) {
        if (command->duration_ms < 100U || command->duration_ms > 1000U) {
            ReadMqttActuators(readback, NULL);
            return "INVALID_PARAM";
        }
        if (g_buzzer_pulse_active != 0U ||
            (g_have_last_buzzer_pulse != 0U &&
             (uint32_t)(now - g_last_buzzer_pulse_tick) < 2000U)) {
            ReadMqttActuators(readback, NULL);
            return "RATE_LIMITED";
        }
        g_buzzer_pulse_active = 1U;
        g_have_last_buzzer_pulse = 1U;
        g_last_buzzer_pulse_tick = now;
        g_buzzer_pulse_off_tick = now + command->duration_ms;
        BEEP_Set(true);
        g_iot_status.buzzer_state = 1U;
    } else if (command->type != MQTT_COMMAND_SYSTEM_GET) {
        ReadMqttActuators(readback, NULL);
        return "INVALID";
    }
    ReadMqttActuators(readback, NULL);
    return "OK";
}

static void BuildMqttState(MqttDeviceState *state)
{
    static const char *const station_names[] = {
        "unknown", "disconnected", "connected"
    };
    WiFiRuntimeInfo runtime;
    static char ip[16];
    size_t index;

    memset(state, 0, sizeof(*state));
    if (WiFi_GetRuntimeInfo(&runtime) &&
        (unsigned int)runtime.station <
            (sizeof(station_names) / sizeof(station_names[0]))) {
        state->station_state = station_names[runtime.station];
    } else {
        memset(&runtime, 0, sizeof(runtime));
        state->station_state = "unknown";
    }
    state->ip_address = NULL;
    if (runtime.ip == WIFI_IP_READY && WiFi_GetStationIp(ip, sizeof(ip))) {
        state->ip_address = ip;
    }
    state->uptime_s = g_iot_status.uptime;
    state->bluetooth_connected = g_iot_status.bt_connected;
    for (index = 0U; index < 3U; index++) {
        state->led[index] = LED_Get((uint8_t)index) ? 1U : 0U;
    }
    state->buzzer_gpio_high = BEEP_Get() ? 1U : 0U;
    state->remote_control_available = g_mqtt_rng_ready;
}

static void ProcessBuzzerPulse(uint32_t now)
{
    if (g_buzzer_pulse_active != 0U &&
        (int32_t)(now - g_buzzer_pulse_off_tick) >= 0) {
        SetBuzzerState(false);
    }
}
#endif

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
#if IOT_MQTT_ENABLE
    char mqtt_session[MQTT_JSON_ID_LENGTH + 1U];
#endif
    memset(&g_iot_status, 0, sizeof(g_iot_status));
    strncpy(g_iot_status.device_id,
            (device_id != NULL && device_id[0] != '\0') ?
                device_id : "f407-touchscreen-01",
            sizeof(g_iot_status.device_id) - 1U);
    g_iot_status.device_id[sizeof(g_iot_status.device_id) - 1U] = '\0';
    g_last_uptime_tick = HAL_GetTick();
    BluetoothCommandParser_Init(&g_bt_parser);
    g_buzzer_pulse_active = 0U;
#if IOT_MQTT_ENABLE
    g_have_last_buzzer_pulse = 0U;
    g_buzzer_pulse_off_tick = 0U;
    g_last_buzzer_pulse_tick = 0U;
    if (!GenerateMqttSessionId(mqtt_session)) {
        (void)strncpy(mqtt_session, "0000000000000000", sizeof(mqtt_session));
        mqtt_session[sizeof(mqtt_session) - 1U] = '\0';
    }
#endif
#if IOT_AUTO_TCP_DIAGNOSTIC_ENABLE
    g_auto_tcp_phase = IOT_AUTO_TCP_WIFI_STATUS_QUERY_REQUEST;
    g_auto_tcp_next_action_tick = HAL_GetTick();
    g_auto_tcp_retry_delay_ms = IOT_AUTO_TCP_RETRY_INITIAL_MS;
#endif
    BT_Init();
    BT_SetRxCallback(BluetoothRxByte);
    WiFi_Init();
#if IOT_MQTT_ENABLE
    MqttClient_Init(g_iot_status.device_id, mqtt_session,
                    g_mqtt_rng_ready != 0U, HandleMqttCommand,
                    ReadMqttActuators, NULL);
#endif
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
#if IOT_MQTT_ENABLE
    {
        MqttDeviceState mqtt_state;
        ProcessBuzzerPulse(now);
        BuildMqttState(&mqtt_state);
        MqttClient_Process(&mqtt_state);
    }
#endif
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
