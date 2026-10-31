#include "mqtt_client.h"
#include "mqtt_wire.h"
#include "wifi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REQUIRE(condition) \
    do { \
        if (!(condition)) { \
            (void)fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); \
            exit(1); \
        } \
    } while (0)

#define TEST_DEVICE_ID "f407-touchscreen-01"
#define TEST_SESSION_ID "0123456789abcdef"
#define TEST_COMMAND_TOPIC "f407/f407-touchscreen-01/cmd"

static uint32_t g_tick;
static uint8_t g_wifi_connected;
static WiFiStatus g_wifi_status;
static WiFiAtStationStatus g_station_status;
static WiFiIpStatus g_ip_status;
static uint8_t g_wifi_status_result_ready;
static WiFiStatus g_wifi_status_result;
static uint32_t g_wifi_status_request_count;
static WiFiAtTcpStatus g_tcp_status;
static uint8_t g_tcp_status_result_ready;
static WiFiAtTcpStatus g_tcp_status_result;
static uint32_t g_tcp_status_request_count;
static uint32_t g_tcp_close_count;
static uint8_t g_operation_ready;
static WiFiTcpOperationResult g_operation_result;
static uint8_t g_tcp_rx[2048];
static size_t g_tcp_rx_length;
static uint8_t g_tcp_tx[MQTT_WIRE_PACKET_CAPACITY];
static size_t g_tcp_tx_length;
static uint32_t g_tcp_connect_count;
static uint32_t g_command_count;
static MqttDeviceState g_state;

static void SetOperationResult(WiFiTcpOperationResult result)
{
    g_operation_result = result;
    g_operation_ready = 1U;
}

uint32_t HAL_GetTick(void)
{
    return g_tick;
}

bool WiFi_GetRuntimeInfo(WiFiRuntimeInfo *info)
{
    if (info == NULL) return false;
    memset(info, 0, sizeof(*info));
    info->station = g_station_status;
    info->ip = g_ip_status;
    info->tcp = g_tcp_status;
    return true;
}

WiFiStatus WiFi_GetStatus(void)
{
    return g_wifi_status;
}

bool WiFi_IsConnected(void)
{
    return g_wifi_connected != 0U;
}

bool WiFi_RequestStatusCheck(void)
{
    g_wifi_status_request_count++;
    g_wifi_status_result_ready = 0U;
    return true;
}

bool WiFi_TakeStatusCheckResult(WiFiStatus *status)
{
    if (status == NULL || g_wifi_status_result_ready == 0U) return false;
    *status = g_wifi_status_result;
    g_wifi_status_result_ready = 0U;
    return true;
}

WiFiAtTcpStatus WiFi_GetTcpStatus(void)
{
    return g_tcp_status;
}

bool WiFi_RequestTcpStatusCheck(void)
{
    g_tcp_status_request_count++;
    g_tcp_status_result_ready = 0U;
    return true;
}

bool WiFi_TakeTcpStatusCheckResult(WiFiAtTcpStatus *status)
{
    if (status == NULL || g_tcp_status_result_ready == 0U) return false;
    *status = g_tcp_status_result;
    g_tcp_status_result_ready = 0U;
    return true;
}

bool WiFi_TcpConnect(const char *host, uint16_t port)
{
    REQUIRE(host != NULL && strcmp(host, "192.168.137.1") == 0);
    REQUIRE(port == 1883U);
    g_tcp_connect_count++;
    g_tcp_status = WIFI_AT_TCP_CONNECTED;
    SetOperationResult(WIFI_TCP_OPERATION_SUCCEEDED);
    return true;
}

bool WiFi_TcpClose(void)
{
    g_tcp_close_count++;
    g_tcp_status = WIFI_AT_TCP_DISCONNECTED;
    SetOperationResult(WIFI_TCP_OPERATION_SUCCEEDED);
    return true;
}

bool WiFi_TcpSend(const uint8_t *data, size_t length)
{
    if (data == NULL || length == 0U || length > sizeof(g_tcp_tx)) return false;
    memcpy(g_tcp_tx, data, length);
    g_tcp_tx_length = length;
    SetOperationResult(WIFI_TCP_OPERATION_SUCCEEDED);
    return true;
}

size_t WiFi_TcpRead(uint8_t *output, size_t capacity)
{
    size_t count;
    if (output == NULL || capacity == 0U || g_tcp_rx_length == 0U) return 0U;
    count = g_tcp_rx_length < capacity ? g_tcp_rx_length : capacity;
    memcpy(output, g_tcp_rx, count);
    g_tcp_rx_length -= count;
    if (g_tcp_rx_length != 0U) {
        memmove(g_tcp_rx, g_tcp_rx + count, g_tcp_rx_length);
    }
    return count;
}

bool WiFi_TakeTcpRxOverflow(void)
{
    return false;
}

bool WiFi_TakeTcpOperationResult(WiFiTcpOperationResult *result)
{
    if (result == NULL || g_operation_ready == 0U) return false;
    *result = g_operation_result;
    g_operation_ready = 0U;
    return true;
}

static void FeedNetwork(const uint8_t *packet, size_t length)
{
    REQUIRE(packet != NULL && length <= sizeof(g_tcp_rx));
    REQUIRE(g_tcp_rx_length == 0U);
    memcpy(g_tcp_rx, packet, length);
    g_tcp_rx_length = length;
}

static void ProcessClient(void)
{
    MqttClient_Process(&g_state);
}

static bool DecodeRemainingLength(const uint8_t *packet, size_t packet_length,
                                  uint32_t *remaining, size_t *header_length)
{
    uint32_t multiplier = 1U;
    uint32_t value = 0U;
    size_t index;
    for (index = 1U; index < packet_length && index <= 4U; index++) {
        uint8_t digit = packet[index];
        value += (uint32_t)(digit & 0x7FU) * multiplier;
        if ((digit & 0x80U) == 0U) {
            *remaining = value;
            *header_length = index + 1U;
            return true;
        }
        multiplier *= 128U;
    }
    return false;
}

static bool DecodePublish(const uint8_t *packet, size_t packet_length,
                          char *topic, size_t topic_capacity,
                          uint16_t *packet_id, const uint8_t **payload,
                          size_t *payload_length, uint8_t *qos, uint8_t *retain)
{
    uint32_t remaining;
    size_t header_length;
    size_t topic_length;
    size_t offset;
    if (packet == NULL || packet_length < 4U || (packet[0] >> 4) != 3U ||
        !DecodeRemainingLength(packet, packet_length, &remaining, &header_length) ||
        header_length + remaining != packet_length || remaining < 2U) {
        return false;
    }
    topic_length = (size_t)(((uint16_t)packet[header_length] << 8) |
                            packet[header_length + 1U]);
    if (topic_length + 2U > remaining || topic_length + 1U > topic_capacity) {
        return false;
    }
    memcpy(topic, packet + header_length + 2U, topic_length);
    topic[topic_length] = '\0';
    offset = header_length + 2U + topic_length;
    *qos = (uint8_t)((packet[0] >> 1) & 0x03U);
    *retain = (uint8_t)(packet[0] & 0x01U);
    *packet_id = 0U;
    if (*qos == 1U) {
        if (offset + 2U > packet_length) return false;
        *packet_id = (uint16_t)(((uint16_t)packet[offset] << 8) | packet[offset + 1U]);
        offset += 2U;
    } else if (*qos != 0U) {
        return false;
    }
    *payload = packet + offset;
    *payload_length = packet_length - offset;
    return true;
}

static bool ContainsBytes(const uint8_t *bytes, size_t length, const char *text)
{
    size_t text_length = strlen(text);
    size_t index;
    if (bytes == NULL || text == NULL || text_length > length) return false;
    for (index = 0U; index <= length - text_length; index++) {
        if (memcmp(bytes + index, text, text_length) == 0) return true;
    }
    return false;
}

static const char *ApplyCommand(const MqttCommand *command,
                                MqttActuatorReadback *readback,
                                void *context)
{
    (void)readback;
    (void)context;
    g_command_count++;
    if (command->type == MQTT_COMMAND_LED_SET && command->led_index < 3U) {
        g_state.led[command->led_index] = command->led_on;
        return "OK";
    }
    return "INVALID_PARAM";
}

static void ReadActuators(MqttActuatorReadback *readback, void *context)
{
    (void)context;
    memcpy(readback->led, g_state.led, sizeof(readback->led));
    readback->buzzer_gpio_high = g_state.buzzer_gpio_high;
}

static void ResetFakeNetwork(void)
{
    g_tick = 0U;
    g_wifi_connected = 1U;
    g_wifi_status = WIFI_STATUS_CONNECTED;
    g_station_status = WIFI_AT_STATION_CONNECTED;
    g_ip_status = WIFI_IP_READY;
    g_wifi_status_result_ready = 0U;
    g_wifi_status_result = WIFI_STATUS_UNKNOWN;
    g_wifi_status_request_count = 0U;
    g_tcp_status = WIFI_AT_TCP_DISCONNECTED;
    g_tcp_status_result_ready = 0U;
    g_tcp_status_result = WIFI_AT_TCP_UNKNOWN;
    g_tcp_status_request_count = 0U;
    g_tcp_close_count = 0U;
    g_operation_ready = 0U;
    g_operation_result = WIFI_TCP_OPERATION_IDLE;
    g_tcp_rx_length = 0U;
    g_tcp_tx_length = 0U;
    g_tcp_connect_count = 0U;
    g_command_count = 0U;
    memset(&g_state, 0, sizeof(g_state));
    g_state.station_state = "connected";
    g_state.ip_address = "192.168.137.101";
    g_state.bluetooth_connected = 1U;
    g_state.remote_control_available = 1U;
}

static void PublishWifiStatus(WiFiStatus status, WiFiAtStationStatus station,
                              WiFiIpStatus ip, WiFiAtTcpStatus tcp)
{
    g_wifi_status = status;
    g_wifi_connected = status == WIFI_STATUS_CONNECTED ? 1U : 0U;
    g_station_status = station;
    g_ip_status = ip;
    g_tcp_status = tcp;
    g_wifi_status_result = status;
    g_wifi_status_result_ready = 1U;
}

static void PublishTcpStatus(WiFiAtTcpStatus status)
{
    g_tcp_status = status;
    g_tcp_status_result = status;
    g_tcp_status_result_ready = 1U;
}

static void TestJsonCodec(void)
{
    static const uint8_t led_json[] =
        "{\"schema_version\":1,\"device_id\":\"f407-touchscreen-01\","
        "\"session_id\":\"0123456789ABCDEF\",\"command_id\":\"ABCDEF0123456789\","
        "\"command\":\"led.set\",\"led_index\":1,\"value\":\"on\"}";
    static const uint8_t bad_duplicate[] =
        "{\"schema_version\":1,\"schema_version\":1,\"device_id\":\"f407-touchscreen-01\","
        "\"session_id\":\"0123456789abcdef\",\"command_id\":\"1111111111111111\","
        "\"command\":\"system.get\"}";
    static const uint8_t old_version[] =
        "{\"schema_version\":2,\"device_id\":\"f407-touchscreen-01\","
        "\"session_id\":\"0123456789abcdef\",\"command_id\":\"1111111111111111\","
        "\"command\":\"system.get\"}";
    static const uint8_t buzzer_json[] =
        "{\"schema_version\":1,\"device_id\":\"f407-touchscreen-01\","
        "\"session_id\":\"0123456789abcdef\",\"command_id\":\"1111111111111111\","
        "\"command\":\"buzzer.pulse\"}";
    MqttCommand command;
    MqttDeviceState state;
    MqttActuatorReadback readback;
    char output[512];
    size_t length;

    REQUIRE(MqttJson_ParseCommand(led_json, sizeof(led_json) - 1U, &command) ==
            MQTT_COMMAND_PARSE_OK);
    REQUIRE(command.type == MQTT_COMMAND_LED_SET && command.led_index == 1U &&
            command.led_on == 1U);
    REQUIRE(strcmp(command.session_id, "0123456789abcdef") == 0);
    REQUIRE(strcmp(command.command_id, "abcdef0123456789") == 0);
    REQUIRE(MqttJson_ParseCommand(bad_duplicate, sizeof(bad_duplicate) - 1U,
                                  &command) == MQTT_COMMAND_PARSE_INVALID);
    REQUIRE(MqttJson_ParseCommand(old_version, sizeof(old_version) - 1U,
                                  &command) == MQTT_COMMAND_PARSE_UNSUPPORTED_VERSION);
    REQUIRE(MqttJson_ParseCommand(buzzer_json, sizeof(buzzer_json) - 1U,
                                  &command) == MQTT_COMMAND_PARSE_OK);
    REQUIRE(command.type == MQTT_COMMAND_BUZZER_PULSE && command.duration_ms == 500U);

    memset(&state, 0, sizeof(state));
    state.station_state = "connected";
    state.ip_address = NULL;
    state.uptime_s = 42U;
    state.bluetooth_connected = 1U;
    state.led[0] = 1U;
    state.buzzer_gpio_high = 0U;
    state.dropped_snapshots = 3U;
    state.remote_control_available = 1U;
    length = MqttJson_BuildState(&state, TEST_DEVICE_ID, TEST_SESSION_ID,
                                 7U, output, sizeof(output));
    REQUIRE(length != 0U && strlen(output) == length);
    REQUIRE(strstr(output, "\"schema_version\":1") != NULL);
    REQUIRE(strstr(output, "\"seq\":7") != NULL);
    REQUIRE(strstr(output, "\"ip\":null") != NULL);
    REQUIRE(strstr(output, "\"dropped_snapshots\":3") != NULL);
    REQUIRE(MqttJson_BuildState(&state, TEST_DEVICE_ID, TEST_SESSION_ID,
                                7U, output, 8U) == 0U);

    memset(&readback, 0, sizeof(readback));
    readback.led[0] = 1U;
    length = MqttJson_BuildAck(&command, TEST_DEVICE_ID, TEST_SESSION_ID,
                               "OK", &readback, output, sizeof(output));
    REQUIRE(length != 0U && strstr(output, "\"result\":\"OK\"") != NULL);
    REQUIRE(strstr(output, "\"buzzer_gpio_high\":false") != NULL);
}

typedef struct {
    size_t count;
    size_t lengths[3];
    uint8_t packets[3][512];
} PacketCapture;

static void CapturePacket(void *context, const uint8_t *packet, size_t length)
{
    PacketCapture *capture = (PacketCapture *)context;
    REQUIRE(capture->count < 3U && length <= sizeof(capture->packets[0]));
    memcpy(capture->packets[capture->count], packet, length);
    capture->lengths[capture->count] = length;
    capture->count++;
}

static void TestWireCodecAndParser(void)
{
    MqttWireParser parser;
    PacketCapture capture = {0};
    uint8_t output[MQTT_WIRE_PACKET_CAPACITY];
    uint8_t long_payload[200];
    const uint8_t simple_packets[] = {0xD0U, 0x00U, 0x40U, 0x02U, 0x00U, 0x09U};
    const uint8_t malformed[] = {0xD0U, 0x80U, 0x00U};
    size_t length;
    size_t index;

    memset(long_payload, 0x5AU, sizeof(long_payload));
    length = MqttWire_BuildConnect(TEST_DEVICE_ID,
        "f407/f407-touchscreen-01/availability", "offline", 30U,
        output, sizeof(output));
    REQUIRE(length > 10U && output[0] == 0x10U);
    REQUIRE(output[2] == 0x00U && output[3] == 0x04U &&
            memcmp(output + 4U, "MQTT", 4U) == 0 && output[8] == 4U);
    REQUIRE(output[9] == 0x2EU);

    length = MqttWire_BuildSubscribe(1U, TEST_COMMAND_TOPIC, 1U,
                                     output, sizeof(output));
    REQUIRE(length > 5U && output[0] == 0x82U);
    REQUIRE(MqttWire_BuildPubAck(9U, output, sizeof(output)) == 4U);
    REQUIRE(output[0] == 0x40U && output[2] == 0U && output[3] == 9U);
    REQUIRE(MqttWire_BuildPingReq(output, sizeof(output)) == 2U);
    REQUIRE(output[0] == 0xC0U && output[1] == 0U);

    length = MqttWire_BuildPublish("f407/test/state", long_payload,
        sizeof(long_payload), 1U, true, false, 0x1234U,
        output, sizeof(output));
    REQUIRE(length > 128U && output[0] == 0x33U);
    MqttWire_Init(&parser);
    for (index = 0U; index < length; index++) {
        REQUIRE(MqttWire_FeedByte(&parser, output[index], CapturePacket,
                                  &capture));
    }
    REQUIRE(capture.count == 1U && capture.lengths[0] == length);
    REQUIRE(memcmp(capture.packets[0], output, length) == 0);

    MqttWire_Init(&parser);
    capture.count = 0U;
    for (index = 0U; index < sizeof(simple_packets); index++) {
        REQUIRE(MqttWire_FeedByte(&parser, simple_packets[index], CapturePacket,
                                  &capture));
    }
    REQUIRE(capture.count == 2U);
    REQUIRE(capture.lengths[0] == 2U && capture.packets[0][0] == 0xD0U);
    REQUIRE(capture.lengths[1] == 4U && capture.packets[1][0] == 0x40U);

    MqttWire_Init(&parser);
    REQUIRE(MqttWire_FeedByte(&parser, malformed[0], CapturePacket, &capture));
    REQUIRE(MqttWire_FeedByte(&parser, malformed[1], CapturePacket, &capture));
    REQUIRE(!MqttWire_FeedByte(&parser, malformed[2], CapturePacket, &capture));
}

static void DriveClientOnline(void)
{
    static const uint8_t connack[] = {0x20U, 0x02U, 0x00U, 0x00U};
    static const uint8_t suback[] = {0x90U, 0x03U, 0x00U, 0x01U, 0x01U};

    MqttClient_Init(TEST_DEVICE_ID, TEST_SESSION_ID, true,
                    ApplyCommand, ReadActuators, NULL);
    ProcessClient();
    REQUIRE(g_wifi_status_request_count == 1U &&
            g_tcp_status_request_count == 0U && g_tcp_connect_count == 0U);
    PublishWifiStatus(WIFI_STATUS_CONNECTED, WIFI_AT_STATION_CONNECTED,
                      WIFI_IP_READY, WIFI_AT_TCP_DISCONNECTED);
    ProcessClient();
    REQUIRE(g_tcp_status_request_count == 1U && g_tcp_connect_count == 0U);
    PublishTcpStatus(WIFI_AT_TCP_DISCONNECTED);
    ProcessClient();
    REQUIRE(g_tcp_connect_count == 1U);
    ProcessClient();
    REQUIRE(g_tcp_tx_length > 10U && g_tcp_tx[0] == 0x10U);
    ProcessClient();
    FeedNetwork(connack, sizeof(connack));
    ProcessClient();
    REQUIRE(g_tcp_tx_length > 5U && g_tcp_tx[0] == 0x82U);
    ProcessClient();
    FeedNetwork(suback, sizeof(suback));
    ProcessClient();
    REQUIRE(MqttClient_IsOnline());
}

static void TestReconnectRequiresFreshNetworkState(void)
{
    ResetFakeNetwork();
    MqttClient_Init(TEST_DEVICE_ID, TEST_SESSION_ID, true,
                    ApplyCommand, ReadActuators, NULL);
    ProcessClient();
    REQUIRE(g_wifi_status_request_count == 1U);
    REQUIRE(g_tcp_status_request_count == 0U && g_tcp_connect_count == 0U);

    /* A stale connected cache must not bypass the fresh disconnected result. */
    PublishWifiStatus(WIFI_STATUS_RETRY_WAIT, WIFI_AT_STATION_DISCONNECTED,
                      WIFI_IP_NOT_READY, WIFI_AT_TCP_DISCONNECTED);
    ProcessClient();
    REQUIRE(g_tcp_status_request_count == 0U && g_tcp_connect_count == 0U);

    g_tick += 1000U;
    ProcessClient();
    REQUIRE(g_wifi_status_request_count == 2U && g_tcp_connect_count == 0U);
    PublishWifiStatus(WIFI_STATUS_CONNECTED, WIFI_AT_STATION_CONNECTED,
                      WIFI_IP_READY, WIFI_AT_TCP_DISCONNECTED);
    ProcessClient();
    REQUIRE(g_tcp_status_request_count == 1U && g_tcp_connect_count == 0U);
    PublishTcpStatus(WIFI_AT_TCP_DISCONNECTED);
    ProcessClient();
    REQUIRE(g_tcp_connect_count == 1U);
}

static void TestFreshStatusRequiresStationAndIpReady(void)
{
    ResetFakeNetwork();
    MqttClient_Init(TEST_DEVICE_ID, TEST_SESSION_ID, true,
                    ApplyCommand, ReadActuators, NULL);
    ProcessClient();
    g_tick += 5000U;
    ProcessClient();
    REQUIRE(g_wifi_status_request_count == 1U &&
            g_tcp_status_request_count == 0U && g_tcp_connect_count == 0U);
    PublishWifiStatus(WIFI_STATUS_CONNECTED, WIFI_AT_STATION_CONNECTED,
                      WIFI_IP_NOT_READY, WIFI_AT_TCP_DISCONNECTED);
    ProcessClient();
    REQUIRE(g_tcp_status_request_count == 0U && g_tcp_connect_count == 0U);
}

static void TestFreshTcpConnectedIsClosedBeforeRetry(void)
{
    ResetFakeNetwork();
    MqttClient_Init(TEST_DEVICE_ID, TEST_SESSION_ID, true,
                    ApplyCommand, ReadActuators, NULL);
    ProcessClient();
    PublishWifiStatus(WIFI_STATUS_CONNECTED, WIFI_AT_STATION_CONNECTED,
                      WIFI_IP_READY, WIFI_AT_TCP_DISCONNECTED);
    ProcessClient();
    PublishTcpStatus(WIFI_AT_TCP_CONNECTED);
    ProcessClient();
    REQUIRE(g_tcp_connect_count == 0U && g_tcp_close_count == 1U);
}

static void FeedPubAck(uint16_t packet_id)
{
    uint8_t packet[4];
    REQUIRE(MqttWire_BuildPubAck(packet_id, packet, sizeof(packet)) == sizeof(packet));
    FeedNetwork(packet, sizeof(packet));
}

static void TestClientStatePublishAndCommandDedup(void)
{
    static const uint8_t command_json[] =
        "{\"schema_version\":1,\"device_id\":\"f407-touchscreen-01\","
        "\"session_id\":\"0123456789abcdef\",\"command_id\":\"1111111111111111\","
        "\"command\":\"led.set\",\"led_index\":1,\"value\":\"on\"}";
    uint8_t packet[MQTT_WIRE_PACKET_CAPACITY];
    uint8_t first_ack_json[MQTT_WIRE_PACKET_CAPACITY];
    size_t first_ack_json_length;
    size_t length;
    uint16_t publish_id;
    uint16_t duplicate_ack_id;
    uint16_t previous_state_id;
    const uint8_t *payload;
    size_t payload_length;
    uint8_t qos;
    uint8_t retain;
    char topic[128];

    ResetFakeNetwork();
    DriveClientOnline();

    ProcessClient();
    REQUIRE(DecodePublish(g_tcp_tx, g_tcp_tx_length, topic, sizeof(topic),
                          &publish_id, &payload, &payload_length, &qos, &retain));
    REQUIRE(strcmp(topic, "f407/f407-touchscreen-01/availability") == 0);
    REQUIRE(qos == 1U && retain == 1U && payload_length == 6U &&
            memcmp(payload, "online", 6U) == 0);
    ProcessClient();
    FeedPubAck(publish_id);
    ProcessClient();
    for (size_t state_index = 0U; state_index < 4U; state_index++) {
        REQUIRE(DecodePublish(g_tcp_tx, g_tcp_tx_length, topic, sizeof(topic),
                              &publish_id, &payload, &payload_length, &qos, &retain));
        REQUIRE(strcmp(topic, "f407/f407-touchscreen-01/state") == 0);
        REQUIRE(qos == 1U && retain == 1U);
        REQUIRE(ContainsBytes(payload, payload_length, "\"schema_version\":1"));
        previous_state_id = publish_id;
        ProcessClient();
        FeedPubAck(previous_state_id);
        ProcessClient();
        if (!DecodePublish(g_tcp_tx, g_tcp_tx_length, topic, sizeof(topic),
                           &publish_id, &payload, &payload_length, &qos, &retain) ||
            strcmp(topic, "f407/f407-touchscreen-01/state") != 0 ||
            publish_id == previous_state_id) {
            break;
        }
        REQUIRE(state_index < 3U);
    }

    length = MqttWire_BuildPublish(TEST_COMMAND_TOPIC, command_json,
        sizeof(command_json) - 1U, 1U, false, false, 0x0042U,
        packet, sizeof(packet));
    REQUIRE(length != 0U);
    REQUIRE(DecodePublish(packet, length, topic, sizeof(topic),
                          &publish_id, &payload, &payload_length, &qos, &retain));
    REQUIRE(strcmp(topic, TEST_COMMAND_TOPIC) == 0 && qos == 1U && retain == 0U);
    REQUIRE(payload_length == sizeof(command_json) - 1U &&
            memcmp(payload, command_json, payload_length) == 0);
    REQUIRE(MqttJson_ParseCommand(payload, payload_length, &(MqttCommand){0}) ==
            MQTT_COMMAND_PARSE_OK);
    FeedNetwork(packet, length);
    ProcessClient();
    REQUIRE(g_command_count == 0U && g_tcp_rx_length != 0U);
    ProcessClient();
    REQUIRE(g_tcp_tx_length == 4U && g_tcp_tx[0] == 0x40U &&
            g_tcp_tx[2] == 0x00U && g_tcp_tx[3] == 0x42U);
    REQUIRE(g_command_count == 1U && g_state.led[1] == 1U);

    ProcessClient();
    REQUIRE(DecodePublish(g_tcp_tx, g_tcp_tx_length, topic, sizeof(topic),
                          &publish_id, &payload, &payload_length, &qos, &retain));
    REQUIRE(strcmp(topic, "f407/f407-touchscreen-01/ack") == 0);
    REQUIRE(qos == 1U && retain == 0U);
    REQUIRE(ContainsBytes(payload, payload_length, "\"result\":\"OK\""));
    REQUIRE(ContainsBytes(payload, payload_length,
                          "\"buzzer_gpio_high\":false"));
    first_ack_json_length = payload_length;
    memcpy(first_ack_json, payload, first_ack_json_length);

    ProcessClient();
    length = MqttWire_BuildPublish(TEST_COMMAND_TOPIC, command_json,
        sizeof(command_json) - 1U, 1U, false, false, 0x0043U,
        packet, sizeof(packet));
    REQUIRE(length != 0U);
    FeedNetwork(packet, length);
    ProcessClient();
    REQUIRE(g_command_count == 1U);
    REQUIRE(g_tcp_rx_length != 0U);
    ProcessClient();
    REQUIRE(g_command_count == 1U && g_tcp_rx_length == 0U);

    FeedPubAck(publish_id);
    ProcessClient();
    REQUIRE(g_tcp_tx_length == 4U && g_tcp_tx[0] == 0x40U &&
            g_tcp_tx[2] == 0x00U && g_tcp_tx[3] == 0x43U);
    ProcessClient();
    REQUIRE(DecodePublish(g_tcp_tx, g_tcp_tx_length, topic, sizeof(topic),
                          &duplicate_ack_id, &payload, &payload_length, &qos, &retain));
    REQUIRE(strcmp(topic, "f407/f407-touchscreen-01/ack") == 0);
    REQUIRE(qos == 1U && retain == 0U);
    REQUIRE(payload_length == first_ack_json_length &&
            memcmp(payload, first_ack_json, first_ack_json_length) == 0);
    REQUIRE(duplicate_ack_id != publish_id && g_command_count == 1U);
}

int main(void)
{
    TestJsonCodec();
    TestWireCodecAndParser();
    TestReconnectRequiresFreshNetworkState();
    TestFreshStatusRequiresStationAndIpReady();
    TestFreshTcpConnectedIsClosedBeforeRetry();
    TestClientStatePublishAndCommandDedup();
    (void)puts("mqtt_core tests passed");
    return 0;
}
