#include "mqtt_client.h"

#include "mqtt_wire.h"
#include "wifi.h"
#include "stm32f4xx_hal.h"

#include <stdio.h>
#include <string.h>

#ifndef IOT_MQTT_BROKER_HOST
#define IOT_MQTT_BROKER_HOST "192.168.137.1"
#endif
#ifndef IOT_MQTT_BROKER_PORT
#define IOT_MQTT_BROKER_PORT 1883U
#endif

#define MQTT_KEEPALIVE_SECONDS          30U
#define MQTT_TCP_RX_CHUNK               128U
#define MQTT_STATE_QUEUE_CAPACITY       16U
#define MQTT_ACK_QUEUE_CAPACITY         4U
#define MQTT_ACK_JSON_CAPACITY          320U
#define MQTT_STATE_JSON_CAPACITY        512U
#define MQTT_MAX_COMMAND_JSON           384U
#define MQTT_DEDUP_CAPACITY             8U
#define MQTT_RETRY_INITIAL_MS           1000U
#define MQTT_RETRY_MAX_MS               30000U
#define MQTT_OPERATION_TIMEOUT_MS       8000U
#define MQTT_PACKET_TIMEOUT_MS          5000U
#define MQTT_WIFI_STATUS_TIMEOUT_MS     10000U
#define MQTT_PING_TIMEOUT_MS            10000U
#define MQTT_STATE_HEARTBEAT_MS         10000U
#define MQTT_PING_INTERVAL_MS           15000U

typedef struct {
    uint32_t uptime_s;
    char station[16];
    char ip[16];
    uint8_t bluetooth_connected;
    uint8_t led[3];
    uint8_t buzzer_gpio_high;
    uint32_t dropped_snapshots;
    uint8_t remote_control_available;
    uint32_t sequence;
} MqttStoredState;

typedef struct {
    uint16_t length;
    uint8_t bytes[MQTT_ACK_JSON_CAPACITY];
} MqttAckItem;

typedef struct {
    uint8_t used;
    char command_id[MQTT_JSON_ID_LENGTH + 1U];
    uint32_t fingerprint;
    uint16_t length;
    uint8_t bytes[MQTT_ACK_JSON_CAPACITY];
} MqttDedupItem;

typedef enum {
    MQTT_PHASE_BACKOFF = 0,
    MQTT_PHASE_WIFI_STATUS_WAIT,
    MQTT_PHASE_TCP_STATUS_WAIT,
    MQTT_PHASE_TCP_CONNECT_WAIT,
    MQTT_PHASE_CONNECT_PACKET_WAIT,
    MQTT_PHASE_CONNACK_WAIT,
    MQTT_PHASE_SUBSCRIBE_PACKET_WAIT,
    MQTT_PHASE_SUBACK_WAIT,
    MQTT_PHASE_ONLINE,
    MQTT_PHASE_CLOSE_WAIT
} MqttPhase;

typedef enum {
    MQTT_TX_NONE = 0,
    MQTT_TX_TCP_CONNECT,
    MQTT_TX_TCP_CLOSE,
    MQTT_TX_CONNECT_PACKET,
    MQTT_TX_SUBSCRIBE_PACKET,
    MQTT_TX_QOS1_PUBLISH,
    MQTT_TX_INBOUND_PUBACK,
    MQTT_TX_PINGREQ
} MqttTxPurpose;

typedef enum {
    MQTT_INFLIGHT_NONE = 0,
    MQTT_INFLIGHT_AVAILABILITY,
    MQTT_INFLIGHT_STATE,
    MQTT_INFLIGHT_ACK
} MqttInflightKind;

typedef struct {
    uint8_t initialized;
    uint8_t online;
    uint8_t remote_control_available;
    uint8_t force_snapshot;
    uint8_t last_state_valid;
    uint8_t availability_pending;
    uint8_t connack_received;
    uint8_t suback_received;
    uint8_t ping_waiting;
    uint8_t wifi_operation_pending;
    uint8_t close_after_operation;
    uint8_t protocol_error;
    uint8_t incoming_puback_count;
    uint8_t incoming_puback_head;
    uint8_t outgoing_inflight;
    uint16_t next_packet_id;
    uint16_t tx_packet_id;
    uint16_t inflight_packet_id;
    uint16_t incoming_puback_ids[MQTT_ACK_QUEUE_CAPACITY];
    uint8_t tx_purpose;
    uint8_t inflight_kind;
    uint32_t phase_deadline;
    uint32_t retry_at;
    uint32_t retry_delay_ms;
    uint32_t inflight_deadline;
    uint32_t ping_deadline;
    uint32_t last_io_tick;
    uint32_t last_snapshot_tick;
    uint32_t dropped_snapshots;
    uint32_t next_sequence;
    char device_id[MQTT_JSON_DEVICE_ID_CAPACITY];
    char session_id[MQTT_JSON_ID_LENGTH + 1U];
    char availability_topic[96];
    char state_topic[96];
    char command_topic[96];
    char ack_topic[96];
    uint8_t tcp_rx[MQTT_TCP_RX_CHUNK];
    uint8_t tx_packet[MQTT_WIRE_PACKET_CAPACITY];
    uint16_t tx_length;
    uint8_t inflight_packet[MQTT_WIRE_PACKET_CAPACITY];
    uint16_t inflight_length;
    uint8_t state_head;
    uint8_t state_count;
    MqttStoredState states[MQTT_STATE_QUEUE_CAPACITY];
    MqttStoredState previous_state;
    uint8_t ack_head;
    uint8_t ack_count;
    MqttAckItem acks[MQTT_ACK_QUEUE_CAPACITY];
    MqttDedupItem dedup[MQTT_DEDUP_CAPACITY];
    uint8_t dedup_next;
    MqttWireParser parser;
    MqttPhase phase;
    MqttCommandHandler command_handler;
    MqttReadbackHandler readback_handler;
    void *handler_context;
} MqttClientContext;

static MqttClientContext g_mqtt;

static uint32_t MqttDeadline(uint32_t now, uint32_t deadline)
{
    return (uint32_t)(now - deadline) < 0x80000000UL;
}

static uint16_t NextPacketId(void)
{
    g_mqtt.next_packet_id++;
    if (g_mqtt.next_packet_id == 0U) {
        g_mqtt.next_packet_id = 1U;
    }
    return g_mqtt.next_packet_id;
}

static uint32_t JsonFingerprint(const uint8_t *bytes, size_t length)
{
    uint32_t hash = 2166136261UL;
    size_t index;
    for (index = 0U; index < length; index++) {
        hash ^= bytes[index];
        hash *= 16777619UL;
    }
    return hash;
}

static void StoreText(char *output, size_t capacity, const char *input)
{
    size_t length;
    if (capacity == 0U) {
        return;
    }
    if (input == NULL) {
        output[0] = '\0';
        return;
    }
    length = strlen(input);
    if (length >= capacity) {
        length = capacity - 1U;
    }
    memcpy(output, input, length);
    output[length] = '\0';
}

static MqttStoredState CopyState(const MqttDeviceState *state)
{
    MqttStoredState stored;
    size_t index;

    memset(&stored, 0, sizeof(stored));
    stored.uptime_s = state->uptime_s;
    StoreText(stored.station, sizeof(stored.station), state->station_state);
    StoreText(stored.ip, sizeof(stored.ip), state->ip_address);
    stored.bluetooth_connected = state->bluetooth_connected;
    for (index = 0U; index < 3U; index++) {
        stored.led[index] = state->led[index];
    }
    stored.buzzer_gpio_high = state->buzzer_gpio_high;
    stored.remote_control_available = state->remote_control_available;
    stored.dropped_snapshots = g_mqtt.dropped_snapshots;
    return stored;
}

static bool StatesEqual(const MqttStoredState *left,
                        const MqttStoredState *right)
{
    return strcmp(left->station, right->station) == 0 &&
           strcmp(left->ip, right->ip) == 0 &&
           left->bluetooth_connected == right->bluetooth_connected &&
           left->led[0] == right->led[0] && left->led[1] == right->led[1] &&
           left->led[2] == right->led[2] &&
           left->buzzer_gpio_high == right->buzzer_gpio_high &&
           left->remote_control_available == right->remote_control_available;
}

static void QueueSnapshot(const MqttDeviceState *state, uint32_t now)
{
    MqttStoredState stored = CopyState(state);
    uint8_t index;

    if (g_mqtt.state_count == MQTT_STATE_QUEUE_CAPACITY) {
        g_mqtt.state_head = (uint8_t)((g_mqtt.state_head + 1U) %
                                      MQTT_STATE_QUEUE_CAPACITY);
        g_mqtt.state_count--;
        g_mqtt.dropped_snapshots++;
        stored.dropped_snapshots = g_mqtt.dropped_snapshots;
    }
    index = (uint8_t)((g_mqtt.state_head + g_mqtt.state_count) %
                      MQTT_STATE_QUEUE_CAPACITY);
    stored.sequence = ++g_mqtt.next_sequence;
    g_mqtt.states[index] = stored;
    g_mqtt.state_count++;
    g_mqtt.previous_state = stored;
    g_mqtt.last_state_valid = 1U;
    g_mqtt.last_snapshot_tick = now;
}

static void ObserveState(const MqttDeviceState *state, uint32_t now)
{
    MqttStoredState current = CopyState(state);
    bool changed = g_mqtt.last_state_valid == 0U ||
                   !StatesEqual(&current, &g_mqtt.previous_state);
    bool heartbeat = (uint32_t)(now - g_mqtt.last_snapshot_tick) >=
                     MQTT_STATE_HEARTBEAT_MS;

    if (changed || heartbeat || g_mqtt.force_snapshot != 0U) {
        QueueSnapshot(state, now);
        g_mqtt.force_snapshot = 0U;
    }
}

static size_t BuildSnapshotJson(const MqttStoredState *snapshot,
                                char *output, size_t capacity)
{
    MqttDeviceState state;
    state.station_state = snapshot->station;
    state.ip_address = snapshot->ip[0] != '\0' ? snapshot->ip : NULL;
    state.uptime_s = snapshot->uptime_s;
    state.bluetooth_connected = snapshot->bluetooth_connected;
    memcpy(state.led, snapshot->led, sizeof(state.led));
    state.buzzer_gpio_high = snapshot->buzzer_gpio_high;
    state.dropped_snapshots = snapshot->dropped_snapshots;
    state.remote_control_available = snapshot->remote_control_available;
    return MqttJson_BuildState(&state, g_mqtt.device_id, g_mqtt.session_id,
                               snapshot->sequence, output, capacity);
}

static bool QueueAckBytes(const uint8_t *bytes, size_t length)
{
    uint8_t index;
    if (bytes == NULL || length == 0U || length > MQTT_ACK_JSON_CAPACITY) {
        return false;
    }
    if (g_mqtt.ack_count == MQTT_ACK_QUEUE_CAPACITY) {
        g_mqtt.ack_head = (uint8_t)((g_mqtt.ack_head + 1U) %
                                    MQTT_ACK_QUEUE_CAPACITY);
        g_mqtt.ack_count--;
    }
    index = (uint8_t)((g_mqtt.ack_head + g_mqtt.ack_count) %
                      MQTT_ACK_QUEUE_CAPACITY);
    memcpy(g_mqtt.acks[index].bytes, bytes, length);
    g_mqtt.acks[index].length = (uint16_t)length;
    g_mqtt.ack_count++;
    return true;
}

static MqttDedupItem *FindDedup(const char *command_id)
{
    size_t index;
    for (index = 0U; index < MQTT_DEDUP_CAPACITY; index++) {
        if (g_mqtt.dedup[index].used != 0U &&
            strcmp(g_mqtt.dedup[index].command_id, command_id) == 0) {
            return &g_mqtt.dedup[index];
        }
    }
    return NULL;
}

static void SaveDedup(const MqttCommand *command, uint32_t fingerprint,
                      const uint8_t *ack, size_t ack_length)
{
    MqttDedupItem *item = &g_mqtt.dedup[g_mqtt.dedup_next];
    g_mqtt.dedup_next = (uint8_t)((g_mqtt.dedup_next + 1U) % MQTT_DEDUP_CAPACITY);
    item->used = 1U;
    StoreText(item->command_id, sizeof(item->command_id), command->command_id);
    item->fingerprint = fingerprint;
    if (ack_length > sizeof(item->bytes)) {
        ack_length = sizeof(item->bytes);
    }
    memcpy(item->bytes, ack, ack_length);
    item->length = (uint16_t)ack_length;
}

static const char *CommandTypeName(MqttCommandType type)
{
    switch (type) {
    case MQTT_COMMAND_LED_SET: return "led.set";
    case MQTT_COMMAND_BUZZER_PULSE: return "buzzer.pulse";
    case MQTT_COMMAND_SYSTEM_GET: return "system.get";
    default: return "unknown";
    }
}

static void ReadbackNow(MqttActuatorReadback *readback)
{
    if (readback == NULL) {
        return;
    }
    memset(readback, 0, sizeof(*readback));
    if (g_mqtt.readback_handler != NULL) {
        g_mqtt.readback_handler(readback, g_mqtt.handler_context);
    }
}

static void QueueCommandAck(const MqttCommand *command,
                            const char *ack_session,
                            const char *result,
                            bool cache,
                            uint32_t fingerprint)
{
    MqttActuatorReadback readback;
    char ack[MQTT_ACK_JSON_CAPACITY];
    size_t length;

    ReadbackNow(&readback);
    length = MqttJson_BuildAck(command, g_mqtt.device_id, ack_session,
                               result, &readback, ack, sizeof(ack));
    if (length == 0U || !QueueAckBytes((const uint8_t *)ack, length)) {
        return;
    }
    if (cache) {
        SaveDedup(command, fingerprint, (const uint8_t *)ack, length);
    }
}

static void HandleCommandPayload(const uint8_t *payload, size_t length)
{
    MqttCommand command;
    MqttCommandParseResult parsed;
    uint32_t fingerprint;
    MqttDedupItem *cached;
    const char *result = "INVALID";

    if (payload == NULL || length == 0U || length > MQTT_MAX_COMMAND_JSON) {
        return;
    }
    parsed = MqttJson_ParseCommand(payload, length, &command);
    if (parsed == MQTT_COMMAND_PARSE_INVALID || command.command_id[0] == '\0' ||
        command.session_id[0] == '\0' || command.command_name[0] == '\0') {
        return;
    }
    fingerprint = JsonFingerprint(payload, length);

    if (strcmp(command.device_id, g_mqtt.device_id) != 0) {
        QueueCommandAck(&command, g_mqtt.session_id, "INVALID", false, fingerprint);
        return;
    }
    if (strcmp(command.session_id, g_mqtt.session_id) != 0) {
        QueueCommandAck(&command, command.session_id, "STALE_SESSION", false,
                        fingerprint);
        return;
    }

    cached = FindDedup(command.command_id);
    if (cached != NULL) {
        if (cached->fingerprint == fingerprint) {
            (void)QueueAckBytes(cached->bytes, cached->length);
        } else {
            QueueCommandAck(&command, g_mqtt.session_id, "INVALID", false,
                            fingerprint);
        }
        return;
    }

    if (parsed == MQTT_COMMAND_PARSE_UNSUPPORTED_VERSION) {
        result = "INVALID";
    } else if (g_mqtt.remote_control_available == 0U) {
        result = "ERROR";
    } else if (command.type == MQTT_COMMAND_UNKNOWN ||
               g_mqtt.command_handler == NULL) {
        result = "INVALID";
    } else {
        result = g_mqtt.command_handler(&command, &(MqttActuatorReadback){0},
                                        g_mqtt.handler_context);
        if (result == NULL) {
            result = "ERROR";
        }
        g_mqtt.force_snapshot = 1U;
    }
    QueueCommandAck(&command, g_mqtt.session_id, result, true, fingerprint);
}

static bool QueueIncomingPubAck(uint16_t packet_id)
{
    uint8_t tail;
    if (packet_id == 0U || g_mqtt.incoming_puback_count == MQTT_ACK_QUEUE_CAPACITY) {
        return false;
    }
    tail = (uint8_t)((g_mqtt.incoming_puback_head + g_mqtt.incoming_puback_count) %
                     MQTT_ACK_QUEUE_CAPACITY);
    g_mqtt.incoming_puback_ids[tail] = packet_id;
    g_mqtt.incoming_puback_count++;
    return true;
}

static uint32_t ReadRemainingLength(const uint8_t *packet, size_t packet_length,
                                    size_t *header_length)
{
    uint32_t multiplier = 1U;
    uint32_t value = 0U;
    size_t index;
    for (index = 1U; index < packet_length && index <= 4U; index++) {
        uint8_t digit = packet[index];
        value += (uint32_t)(digit & 0x7FU) * multiplier;
        if ((digit & 0x80U) == 0U) {
            *header_length = index + 1U;
            return value;
        }
        multiplier *= 128U;
    }
    *header_length = 0U;
    return UINT32_MAX;
}

static uint16_t ReadUint16(const uint8_t *bytes)
{
    return (uint16_t)(((uint16_t)bytes[0] << 8) | bytes[1]);
}

static bool IsTopicEqual(const uint8_t *topic, size_t topic_length,
                         const char *expected)
{
    size_t expected_length = strlen(expected);
    return topic_length == expected_length &&
           memcmp(topic, expected, expected_length) == 0;
}

static void ProcessMqttPacket(void *context, const uint8_t *packet,
                              size_t packet_length)
{
    MqttClientContext *mqtt = (MqttClientContext *)context;
    uint8_t type;
    size_t header_length;
    uint32_t remaining_length;
    const uint8_t *body;

    if (mqtt->protocol_error != 0U) {
        return;
    }
    if (packet == NULL || packet_length < 2U) {
        mqtt->protocol_error = 1U;
        return;
    }
    remaining_length = ReadRemainingLength(packet, packet_length, &header_length);
    if (header_length == 0U || header_length + remaining_length != packet_length) {
        mqtt->protocol_error = 1U;
        return;
    }
    type = (uint8_t)(packet[0] >> 4);
    body = packet + header_length;
    mqtt->last_io_tick = HAL_GetTick();

    if (type == 2U) { /* CONNACK */
        if (packet[0] != 0x20U || remaining_length != 2U ||
            body[0] != 0U || body[1] != 0U) {
            mqtt->protocol_error = 1U;
            return;
        }
        mqtt->connack_received = 1U;
    } else if (type == 9U) { /* SUBACK */
        if (packet[0] != 0x90U || remaining_length != 3U ||
            ReadUint16(body) != 1U || body[2] > 1U) {
            mqtt->protocol_error = 1U;
            return;
        }
        mqtt->suback_received = (uint8_t)(body[2] == 1U ? 1U : 2U);
    } else if (type == 4U) { /* PUBACK */
        uint16_t packet_id;
        if (packet[0] != 0x40U || remaining_length != 2U ||
            mqtt->outgoing_inflight == 0U) {
            mqtt->protocol_error = 1U;
            return;
        }
        packet_id = ReadUint16(body);
        if (packet_id != mqtt->inflight_packet_id) {
            mqtt->protocol_error = 1U;
            return;
        }
        if (mqtt->inflight_kind == MQTT_INFLIGHT_AVAILABILITY) {
            mqtt->availability_pending = 0U;
        } else if (mqtt->inflight_kind == MQTT_INFLIGHT_STATE &&
                   mqtt->state_count != 0U) {
            mqtt->state_head = (uint8_t)((mqtt->state_head + 1U) %
                                         MQTT_STATE_QUEUE_CAPACITY);
            mqtt->state_count--;
        } else if (mqtt->inflight_kind == MQTT_INFLIGHT_ACK &&
                   mqtt->ack_count != 0U) {
            mqtt->ack_head = (uint8_t)((mqtt->ack_head + 1U) %
                                       MQTT_ACK_QUEUE_CAPACITY);
            mqtt->ack_count--;
        }
        mqtt->outgoing_inflight = 0U;
        mqtt->inflight_kind = MQTT_INFLIGHT_NONE;
    } else if (type == 3U) { /* PUBLISH */
        uint8_t qos = (uint8_t)((packet[0] >> 1) & 0x03U);
        uint8_t retained = (uint8_t)(packet[0] & 0x01U);
        uint16_t topic_length;
        size_t payload_offset;
        uint16_t packet_id = 0U;

        if (qos == 3U || remaining_length < 2U) {
            mqtt->protocol_error = 1U;
            return;
        }
        topic_length = ReadUint16(body);
        if ((size_t)topic_length + 2U > remaining_length) {
            mqtt->protocol_error = 1U;
            return;
        }
        payload_offset = 2U + topic_length;
        if (qos == 1U) {
            if (payload_offset + 2U > remaining_length) {
                mqtt->protocol_error = 1U;
                return;
            }
            packet_id = ReadUint16(body + payload_offset);
            payload_offset += 2U;
            if (!QueueIncomingPubAck(packet_id)) {
                mqtt->protocol_error = 1U;
                return;
            }
        } else if (qos != 0U) {
            mqtt->protocol_error = 1U;
            return;
        }
        if (IsTopicEqual(body + 2U, topic_length, mqtt->command_topic)) {
            if (qos != 1U || retained != 0U) {
                mqtt->protocol_error = 1U;
                return;
            }
            HandleCommandPayload(body + payload_offset,
                                 remaining_length - payload_offset);
        }
    } else if (type == 13U) { /* PINGRESP */
        if (packet[0] != 0xD0U || remaining_length != 0U) {
            mqtt->protocol_error = 1U;
            return;
        }
        mqtt->ping_waiting = 0U;
    } else {
        mqtt->protocol_error = 1U;
    }
}

static void ScheduleRetry(uint32_t now)
{
    uint32_t delay = g_mqtt.retry_delay_ms;
    if (delay < MQTT_RETRY_INITIAL_MS) {
        delay = MQTT_RETRY_INITIAL_MS;
    }
    g_mqtt.phase = MQTT_PHASE_BACKOFF;
    g_mqtt.retry_at = now + delay;
    if (delay < MQTT_RETRY_MAX_MS) {
        delay *= 2U;
        if (delay > MQTT_RETRY_MAX_MS) {
            delay = MQTT_RETRY_MAX_MS;
        }
    }
    g_mqtt.retry_delay_ms = delay;
    g_mqtt.online = 0U;
    g_mqtt.connack_received = 0U;
    g_mqtt.suback_received = 0U;
    g_mqtt.ping_waiting = 0U;
    g_mqtt.outgoing_inflight = 0U;
    g_mqtt.inflight_kind = MQTT_INFLIGHT_NONE;
    g_mqtt.inflight_length = 0U;
    g_mqtt.wifi_operation_pending = 0U;
    g_mqtt.close_after_operation = 0U;
    g_mqtt.protocol_error = 0U;
    MqttWire_Init(&g_mqtt.parser);
}

static void BeginClose(uint32_t now)
{
    WiFiTcpOperationResult ignored;
    g_mqtt.online = 0U;
    g_mqtt.protocol_error = 0U;
    g_mqtt.incoming_puback_count = 0U;
    g_mqtt.incoming_puback_head = 0U;
    MqttWire_Init(&g_mqtt.parser);
    if (g_mqtt.wifi_operation_pending != 0U) {
        g_mqtt.close_after_operation = 1U;
        g_mqtt.phase = MQTT_PHASE_CLOSE_WAIT;
        return;
    }
    if (!g_mqtt.wifi_operation_pending &&
        WiFi_GetTcpStatus() == WIFI_AT_TCP_CONNECTED &&
        WiFi_TcpClose()) {
        g_mqtt.tx_purpose = MQTT_TX_TCP_CLOSE;
        g_mqtt.wifi_operation_pending = 1U;
        g_mqtt.phase = MQTT_PHASE_CLOSE_WAIT;
        g_mqtt.phase_deadline = now + MQTT_OPERATION_TIMEOUT_MS;
        return;
    }
    while (WiFi_TakeTcpOperationResult(&ignored)) {
        /* Drain a stale operation result before starting the backoff. */
    }
    g_mqtt.wifi_operation_pending = 0U;
    ScheduleRetry(now);
}

static bool BeginSend(uint16_t length, MqttTxPurpose purpose,
                      uint16_t packet_id, uint32_t now)
{
    if (length == 0U || length > sizeof(g_mqtt.tx_packet) ||
        !WiFi_TcpSend(g_mqtt.tx_packet, length)) {
        return false;
    }
    g_mqtt.tx_length = length;
    g_mqtt.tx_purpose = (uint8_t)purpose;
    g_mqtt.tx_packet_id = packet_id;
    g_mqtt.wifi_operation_pending = 1U;
    g_mqtt.phase_deadline = now + MQTT_OPERATION_TIMEOUT_MS;
    return true;
}

static bool StartTcpConnect(uint32_t now)
{
    if (!WiFi_TcpConnect(IOT_MQTT_BROKER_HOST, IOT_MQTT_BROKER_PORT)) {
        return false;
    }
    g_mqtt.tx_purpose = MQTT_TX_TCP_CONNECT;
    g_mqtt.wifi_operation_pending = 1U;
    g_mqtt.phase = MQTT_PHASE_TCP_CONNECT_WAIT;
    g_mqtt.phase_deadline = now + MQTT_OPERATION_TIMEOUT_MS;
    return true;
}

static bool StartTcpClose(uint32_t now)
{
    if (!WiFi_TcpClose()) {
        return false;
    }
    g_mqtt.tx_purpose = MQTT_TX_TCP_CLOSE;
    g_mqtt.wifi_operation_pending = 1U;
    g_mqtt.phase = MQTT_PHASE_CLOSE_WAIT;
    g_mqtt.phase_deadline = now + MQTT_OPERATION_TIMEOUT_MS;
    return true;
}

static bool StartPublish(const char *topic, const uint8_t *payload,
                         size_t payload_length, MqttInflightKind kind,
                         uint32_t now)
{
    uint16_t packet_id = NextPacketId();
    size_t packet_length = MqttWire_BuildPublish(
        topic, payload, payload_length, 1U, kind != MQTT_INFLIGHT_ACK,
        false, packet_id, g_mqtt.tx_packet, sizeof(g_mqtt.tx_packet));

    if (packet_length == 0U) {
        return false;
    }
    g_mqtt.inflight_packet_id = packet_id;
    g_mqtt.inflight_kind = (uint8_t)kind;
    g_mqtt.inflight_length = (uint16_t)packet_length;
    memcpy(g_mqtt.inflight_packet, g_mqtt.tx_packet, packet_length);
    g_mqtt.outgoing_inflight = 1U;
    g_mqtt.inflight_deadline = now + MQTT_PACKET_TIMEOUT_MS;
    if (!BeginSend((uint16_t)packet_length, MQTT_TX_QOS1_PUBLISH,
                   packet_id, now)) {
        g_mqtt.outgoing_inflight = 0U;
        g_mqtt.inflight_kind = MQTT_INFLIGHT_NONE;
        g_mqtt.inflight_length = 0U;
        return false;
    }
    return true;
}

static void PopIncomingPubAck(void)
{
    if (g_mqtt.incoming_puback_count != 0U) {
        g_mqtt.incoming_puback_head = (uint8_t)(
            (g_mqtt.incoming_puback_head + 1U) % MQTT_ACK_QUEUE_CAPACITY);
        g_mqtt.incoming_puback_count--;
    }
}

static void ProcessWifiOperation(uint32_t now)
{
    WiFiTcpOperationResult result;
    MqttTxPurpose purpose;

    if (g_mqtt.wifi_operation_pending == 0U) {
        return;
    }
    if (!WiFi_TakeTcpOperationResult(&result)) {
        if (MqttDeadline(now, g_mqtt.phase_deadline)) {
            BeginClose(now);
        }
        return;
    }
    purpose = (MqttTxPurpose)g_mqtt.tx_purpose;
    g_mqtt.wifi_operation_pending = 0U;
    g_mqtt.tx_purpose = MQTT_TX_NONE;
    if (g_mqtt.close_after_operation != 0U) {
        g_mqtt.close_after_operation = 0U;
        BeginClose(now);
        return;
    }
    if (result != WIFI_TCP_OPERATION_SUCCEEDED) {
        BeginClose(now);
        return;
    }
    g_mqtt.last_io_tick = now;

    if (purpose == MQTT_TX_TCP_CONNECT) {
        size_t packet_length = MqttWire_BuildConnect(
            g_mqtt.device_id, g_mqtt.availability_topic, "offline",
            MQTT_KEEPALIVE_SECONDS, g_mqtt.tx_packet, sizeof(g_mqtt.tx_packet));
        if (packet_length == 0U ||
            !BeginSend((uint16_t)packet_length, MQTT_TX_CONNECT_PACKET, 0U, now)) {
            BeginClose(now);
            return;
        }
        g_mqtt.phase = MQTT_PHASE_CONNECT_PACKET_WAIT;
        return;
    }
    if (purpose == MQTT_TX_TCP_CLOSE) {
        ScheduleRetry(now);
        return;
    }
    if (purpose == MQTT_TX_CONNECT_PACKET) {
        g_mqtt.phase = MQTT_PHASE_CONNACK_WAIT;
        g_mqtt.phase_deadline = now + MQTT_PACKET_TIMEOUT_MS;
        return;
    }
    if (purpose == MQTT_TX_SUBSCRIBE_PACKET) {
        g_mqtt.phase = MQTT_PHASE_SUBACK_WAIT;
        g_mqtt.phase_deadline = now + MQTT_PACKET_TIMEOUT_MS;
        return;
    }
    if (purpose == MQTT_TX_INBOUND_PUBACK) {
        if (g_mqtt.incoming_puback_count != 0U &&
            g_mqtt.incoming_puback_ids[g_mqtt.incoming_puback_head] ==
                g_mqtt.tx_packet_id) {
            PopIncomingPubAck();
        }
        return;
    }
    if (purpose == MQTT_TX_PINGREQ) {
        g_mqtt.ping_waiting = 1U;
        g_mqtt.ping_deadline = now + MQTT_PING_TIMEOUT_MS;
    }
}

static void PollTcpReceive(void)
{
    size_t count = WiFi_TcpRead(g_mqtt.tcp_rx, sizeof(g_mqtt.tcp_rx));
    size_t index;
    if (count == 0U) {
        return;
    }
    for (index = 0U; index < count; index++) {
        if (!MqttWire_FeedByte(&g_mqtt.parser, g_mqtt.tcp_rx[index],
                               ProcessMqttPacket, &g_mqtt)) {
            g_mqtt.protocol_error = 1U;
            return;
        }
    }
}

static void StartSubscribe(uint32_t now)
{
    size_t length = MqttWire_BuildSubscribe(1U, g_mqtt.command_topic, 1U,
                                             g_mqtt.tx_packet,
                                             sizeof(g_mqtt.tx_packet));
    if (length == 0U || !BeginSend((uint16_t)length,
                                   MQTT_TX_SUBSCRIBE_PACKET, 1U, now)) {
        BeginClose(now);
        return;
    }
    g_mqtt.phase = MQTT_PHASE_SUBSCRIBE_PACKET_WAIT;
}

static void ProcessConnection(uint32_t now)
{
    WiFiRuntimeInfo runtime;
    WiFiStatus wifi_status;

    if (g_mqtt.phase == MQTT_PHASE_CLOSE_WAIT) {
        ProcessWifiOperation(now);
        return;
    }
    if (g_mqtt.phase == MQTT_PHASE_TCP_STATUS_WAIT) {
        WiFiAtTcpStatus status;
        if (WiFi_TakeTcpStatusCheckResult(&status)) {
            if (status == WIFI_AT_TCP_CONNECTED) {
                if (!StartTcpClose(now)) {
                    ScheduleRetry(now);
                }
            } else if (status == WIFI_AT_TCP_DISCONNECTED) {
                if (WiFi_GetStatus() != WIFI_STATUS_CONNECTED ||
                    !WiFi_IsConnected() || !WiFi_GetRuntimeInfo(&runtime) ||
                    runtime.station != WIFI_AT_STATION_CONNECTED ||
                    runtime.ip != WIFI_IP_READY) {
                    ScheduleRetry(now);
                    return;
                }
                if (!StartTcpConnect(now)) {
                    g_mqtt.phase = MQTT_PHASE_BACKOFF;
                    g_mqtt.retry_at = now + 500U;
                }
            } else {
                ScheduleRetry(now);
            }
        } else if (MqttDeadline(now, g_mqtt.phase_deadline)) {
            ScheduleRetry(now);
        }
        return;
    }
    if (g_mqtt.phase == MQTT_PHASE_WIFI_STATUS_WAIT) {
        if (WiFi_TakeStatusCheckResult(&wifi_status)) {
            if (wifi_status != WIFI_STATUS_CONNECTED ||
                WiFi_GetStatus() != WIFI_STATUS_CONNECTED ||
                !WiFi_IsConnected() || !WiFi_GetRuntimeInfo(&runtime) ||
                runtime.station != WIFI_AT_STATION_CONNECTED ||
                runtime.ip != WIFI_IP_READY) {
                ScheduleRetry(now);
                return;
            }
            if (WiFi_RequestTcpStatusCheck()) {
                g_mqtt.phase = MQTT_PHASE_TCP_STATUS_WAIT;
                g_mqtt.phase_deadline = now + MQTT_PACKET_TIMEOUT_MS;
            } else {
                g_mqtt.phase = MQTT_PHASE_BACKOFF;
                g_mqtt.retry_at = now + 500U;
            }
        } else if (MqttDeadline(now, g_mqtt.phase_deadline)) {
            ScheduleRetry(now);
        }
        return;
    }
    if (g_mqtt.phase == MQTT_PHASE_TCP_CONNECT_WAIT) {
        ProcessWifiOperation(now);
        return;
    }
    if (g_mqtt.phase == MQTT_PHASE_CONNECT_PACKET_WAIT ||
        g_mqtt.phase == MQTT_PHASE_SUBSCRIBE_PACKET_WAIT) {
        ProcessWifiOperation(now);
        return;
    }
    if (g_mqtt.phase == MQTT_PHASE_CONNACK_WAIT) {
        if (g_mqtt.connack_received != 0U) {
            g_mqtt.connack_received = 0U;
            StartSubscribe(now);
        } else if (MqttDeadline(now, g_mqtt.phase_deadline)) {
            BeginClose(now);
        }
        return;
    }
    if (g_mqtt.phase == MQTT_PHASE_SUBACK_WAIT) {
        if (g_mqtt.suback_received != 0U) {
            if (g_mqtt.suback_received == 2U) {
                BeginClose(now);
                return;
            }
            g_mqtt.suback_received = 0U;
            g_mqtt.phase = MQTT_PHASE_ONLINE;
            g_mqtt.online = 1U;
            g_mqtt.retry_delay_ms = MQTT_RETRY_INITIAL_MS;
            g_mqtt.availability_pending = 1U;
            g_mqtt.force_snapshot = 1U;
            return;
        }
        if (MqttDeadline(now, g_mqtt.phase_deadline)) {
            BeginClose(now);
        }
        return;
    }
    if (g_mqtt.phase == MQTT_PHASE_BACKOFF) {
        if (!MqttDeadline(now, g_mqtt.retry_at)) {
            return;
        }
        if (WiFi_RequestStatusCheck()) {
            g_mqtt.phase = MQTT_PHASE_WIFI_STATUS_WAIT;
            g_mqtt.phase_deadline = now + MQTT_WIFI_STATUS_TIMEOUT_MS;
        } else {
            g_mqtt.retry_at = now + 500U;
        }
    }
}

static bool SendInboundPubAck(uint32_t now)
{
    uint16_t packet_id;
    size_t length;
    if (g_mqtt.incoming_puback_count == 0U) {
        return false;
    }
    packet_id = g_mqtt.incoming_puback_ids[g_mqtt.incoming_puback_head];
    length = MqttWire_BuildPubAck(packet_id, g_mqtt.tx_packet,
                                  sizeof(g_mqtt.tx_packet));
    if (length == 0U) {
        return false;
    }
    return BeginSend((uint16_t)length, MQTT_TX_INBOUND_PUBACK, packet_id, now);
}

static bool SendPingReq(uint32_t now)
{
    size_t length = MqttWire_BuildPingReq(g_mqtt.tx_packet,
                                          sizeof(g_mqtt.tx_packet));
    if (length == 0U) {
        return false;
    }
    if (!BeginSend((uint16_t)length, MQTT_TX_PINGREQ, 0U, now)) {
        return false;
    }
    g_mqtt.ping_waiting = 1U;
    g_mqtt.ping_deadline = now + MQTT_PING_TIMEOUT_MS;
    return true;
}

static void ProcessOnlineOutput(const MqttDeviceState *state, uint32_t now)
{
    char payload[MQTT_STATE_JSON_CAPACITY];
    size_t payload_length;

    if (g_mqtt.wifi_operation_pending != 0U ||
        g_mqtt.outgoing_inflight != 0U) {
        return;
    }
    if (g_mqtt.incoming_puback_count != 0U) {
        (void)SendInboundPubAck(now);
        return;
    }
    if (g_mqtt.ack_count != 0U) {
        MqttAckItem *item = &g_mqtt.acks[g_mqtt.ack_head];
        (void)StartPublish(g_mqtt.ack_topic, item->bytes, item->length,
                           MQTT_INFLIGHT_ACK, now);
        return;
    }
    if (g_mqtt.availability_pending != 0U) {
        static const uint8_t online[] = "online";
        (void)StartPublish(g_mqtt.availability_topic, online,
                           sizeof(online) - 1U, MQTT_INFLIGHT_AVAILABILITY, now);
        return;
    }
    if (g_mqtt.state_count != 0U) {
        MqttStoredState *snapshot = &g_mqtt.states[g_mqtt.state_head];
        payload_length = BuildSnapshotJson(snapshot, payload, sizeof(payload));
        if (payload_length != 0U) {
            (void)StartPublish(g_mqtt.state_topic, (const uint8_t *)payload,
                               payload_length, MQTT_INFLIGHT_STATE, now);
        } else {
            BeginClose(now);
        }
        return;
    }
    if (g_mqtt.ping_waiting != 0U) {
        if (MqttDeadline(now, g_mqtt.ping_deadline)) {
            BeginClose(now);
        }
        return;
    }
    if ((uint32_t)(now - g_mqtt.last_io_tick) >= MQTT_PING_INTERVAL_MS) {
        (void)SendPingReq(now);
    }
    (void)state;
}

void MqttClient_Init(const char *device_id, const char *session_id,
                     bool remote_control_available,
                     MqttCommandHandler command_handler,
                     MqttReadbackHandler readback_handler,
                     void *context)
{
    int length;
    uint32_t now = HAL_GetTick();

    memset(&g_mqtt, 0, sizeof(g_mqtt));
    StoreText(g_mqtt.device_id, sizeof(g_mqtt.device_id), device_id);
    StoreText(g_mqtt.session_id, sizeof(g_mqtt.session_id), session_id);
    length = snprintf(g_mqtt.availability_topic, sizeof(g_mqtt.availability_topic),
                      "f407/%s/availability", g_mqtt.device_id);
    if (length < 0 || (size_t)length >= sizeof(g_mqtt.availability_topic)) return;
    length = snprintf(g_mqtt.state_topic, sizeof(g_mqtt.state_topic),
                      "f407/%s/state", g_mqtt.device_id);
    if (length < 0 || (size_t)length >= sizeof(g_mqtt.state_topic)) return;
    length = snprintf(g_mqtt.command_topic, sizeof(g_mqtt.command_topic),
                      "f407/%s/cmd", g_mqtt.device_id);
    if (length < 0 || (size_t)length >= sizeof(g_mqtt.command_topic)) return;
    length = snprintf(g_mqtt.ack_topic, sizeof(g_mqtt.ack_topic),
                      "f407/%s/ack", g_mqtt.device_id);
    if (length < 0 || (size_t)length >= sizeof(g_mqtt.ack_topic)) return;

    g_mqtt.remote_control_available = remote_control_available ? 1U : 0U;
    g_mqtt.command_handler = command_handler;
    g_mqtt.readback_handler = readback_handler;
    g_mqtt.handler_context = context;
    g_mqtt.retry_delay_ms = MQTT_RETRY_INITIAL_MS;
    g_mqtt.retry_at = now;
    g_mqtt.phase = MQTT_PHASE_BACKOFF;
    g_mqtt.last_io_tick = now;
    g_mqtt.last_snapshot_tick = now - MQTT_STATE_HEARTBEAT_MS;
    g_mqtt.next_packet_id = 1U;
    g_mqtt.initialized = 1U;
    MqttWire_Init(&g_mqtt.parser);
}

void MqttClient_Process(const MqttDeviceState *state)
{
    uint32_t now = HAL_GetTick();

    if (g_mqtt.initialized == 0U || state == NULL) {
        return;
    }
    PollTcpReceive();
    if (WiFi_TakeTcpRxOverflow()) {
        BeginClose(now);
    }
    if (g_mqtt.protocol_error != 0U) {
        BeginClose(now);
    }
    ObserveState(state, now);
    ProcessWifiOperation(now);
    if (g_mqtt.phase == MQTT_PHASE_ONLINE) {
        if (!WiFi_IsConnected() || WiFi_GetTcpStatus() != WIFI_AT_TCP_CONNECTED) {
            BeginClose(now);
        } else if (g_mqtt.outgoing_inflight != 0U &&
                   MqttDeadline(now, g_mqtt.inflight_deadline)) {
            BeginClose(now);
        } else {
            ProcessOnlineOutput(state, now);
        }
    } else {
        ProcessConnection(now);
    }
}

bool MqttClient_IsOnline(void)
{
    return g_mqtt.online != 0U;
}
