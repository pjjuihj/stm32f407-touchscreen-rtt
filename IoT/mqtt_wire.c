#include "mqtt_wire.h"

#include <string.h>

typedef enum {
    MQTT_PACKET_INCOMPLETE = 0,
    MQTT_PACKET_COMPLETE,
    MQTT_PACKET_INVALID
} MqttPacketParseResult;

static bool AppendByte(uint8_t *output, size_t capacity, size_t *position,
                       uint8_t value)
{
    if (*position >= capacity) {
        return false;
    }
    output[(*position)++] = value;
    return true;
}

static bool AppendUint16(uint8_t *output, size_t capacity, size_t *position,
                         uint16_t value)
{
    return AppendByte(output, capacity, position, (uint8_t)(value >> 8)) &&
           AppendByte(output, capacity, position, (uint8_t)value);
}

static bool AppendUtf8(uint8_t *output, size_t capacity, size_t *position,
                       const char *value)
{
    size_t length;

    if (value == NULL) {
        return false;
    }
    length = strlen(value);
    if (length > UINT16_MAX || *position > capacity ||
        capacity - *position < 2U ||
        length > capacity - *position - 2U) {
        return false;
    }
    if (!AppendUint16(output, capacity, position, (uint16_t)length)) {
        return false;
    }
    memcpy(output + *position, value, length);
    *position += length;
    return true;
}

static size_t EncodeRemainingLength(size_t value, uint8_t output[4])
{
    size_t count = 0U;

    do {
        uint8_t digit = (uint8_t)(value % 128U);
        value /= 128U;
        if (value != 0U) {
            digit |= 0x80U;
        }
        output[count++] = digit;
    } while (value != 0U && count < 4U);
    return value == 0U ? count : 0U;
}

static size_t FinishPacket(uint8_t header, uint8_t *output, size_t capacity,
                           size_t body_start, size_t end)
{
    uint8_t encoded_length[4];
    size_t body_length;
    size_t encoded_count;
    size_t total;

    if (capacity < 5U || body_start != 5U || end < body_start) {
        return 0U;
    }
    body_length = end - body_start;
    encoded_count = EncodeRemainingLength(body_length, encoded_length);
    total = 1U + encoded_count + body_length;
    if (encoded_count == 0U || total > capacity) {
        return 0U;
    }
    memmove(output + 1U + encoded_count, output + body_start, body_length);
    output[0] = header;
    memcpy(output + 1U, encoded_length, encoded_count);
    return total;
}

static MqttPacketParseResult DecodeRemainingLength(const uint8_t *bytes,
                                                    size_t length,
                                                    uint32_t *value,
                                                    size_t *encoded_length)
{
    uint32_t multiplier = 1U;
    uint32_t result = 0U;
    size_t index;

    if (length < 2U) {
        return MQTT_PACKET_INCOMPLETE;
    }
    for (index = 1U; index < length && index <= 4U; index++) {
        uint8_t digit = bytes[index];
        result += (uint32_t)(digit & 0x7FU) * multiplier;
        if ((digit & 0x80U) == 0U) {
            if (index > 1U && digit == 0U) {
                return MQTT_PACKET_INVALID;
            }
            *value = result;
            *encoded_length = index;
            return MQTT_PACKET_COMPLETE;
        }
        multiplier *= 128U;
    }
    if (index > 4U || length > 4U) {
        return MQTT_PACKET_INVALID;
    }
    return MQTT_PACKET_INCOMPLETE;
}

static bool TryEmitPackets(MqttWireParser *parser, MqttWirePacketFn packet_fn,
                           void *context)
{
    for (;;) {
        uint32_t remaining_length;
        size_t encoded_length;
        size_t header_length;
        size_t packet_length;
        MqttPacketParseResult result;

        if (parser->length == 0U) {
            return true;
        }
        result = DecodeRemainingLength(parser->bytes, parser->length,
                                       &remaining_length, &encoded_length);
        if (result == MQTT_PACKET_INVALID) {
            return false;
        }
        if (result == MQTT_PACKET_INCOMPLETE) {
            return true;
        }
        header_length = 1U + encoded_length;
        packet_length = header_length + (size_t)remaining_length;
        if (packet_length > sizeof(parser->bytes)) {
            return false;
        }
        if (parser->length < packet_length) {
            return true;
        }
        if (packet_fn != NULL) {
            packet_fn(context, parser->bytes, packet_length);
        }
        parser->length = (uint16_t)(parser->length - packet_length);
        if (parser->length != 0U) {
            memmove(parser->bytes, parser->bytes + packet_length, parser->length);
        }
    }
}

void MqttWire_Init(MqttWireParser *parser)
{
    if (parser != NULL) {
        memset(parser, 0, sizeof(*parser));
    }
}

bool MqttWire_FeedByte(MqttWireParser *parser, uint8_t byte,
                       MqttWirePacketFn packet_fn, void *context)
{
    if (parser == NULL || parser->length >= sizeof(parser->bytes)) {
        return false;
    }
    parser->bytes[parser->length++] = byte;
    return TryEmitPackets(parser, packet_fn, context);
}

size_t MqttWire_BuildConnect(const char *client_id, const char *will_topic,
                             const char *will_payload, uint16_t keep_alive_s,
                             uint8_t *output, size_t capacity)
{
    size_t position = 5U;

    if (output == NULL || capacity < 5U || keep_alive_s == 0U ||
        !AppendUtf8(output, capacity, &position, "MQTT") ||
        !AppendByte(output, capacity, &position, 4U) ||
        !AppendByte(output, capacity, &position, 0x2EU) ||
        !AppendUint16(output, capacity, &position, keep_alive_s) ||
        !AppendUtf8(output, capacity, &position, client_id) ||
        !AppendUtf8(output, capacity, &position, will_topic) ||
        !AppendUtf8(output, capacity, &position, will_payload)) {
        return 0U;
    }
    return FinishPacket(0x10U, output, capacity, 5U, position);
}

size_t MqttWire_BuildSubscribe(uint16_t packet_id, const char *topic,
                               uint8_t qos, uint8_t *output, size_t capacity)
{
    size_t position = 5U;

    if (output == NULL || capacity < 5U || packet_id == 0U || qos > 1U ||
        !AppendUint16(output, capacity, &position, packet_id) ||
        !AppendUtf8(output, capacity, &position, topic) ||
        !AppendByte(output, capacity, &position, qos)) {
        return 0U;
    }
    return FinishPacket(0x82U, output, capacity, 5U, position);
}

size_t MqttWire_BuildPublish(const char *topic, const uint8_t *payload,
                             size_t payload_length, uint8_t qos, bool retain,
                             bool duplicate, uint16_t packet_id,
                             uint8_t *output, size_t capacity)
{
    size_t position = 5U;
    uint8_t header;

    if (output == NULL || capacity < 5U || payload == NULL || qos > 1U ||
        (qos == 1U && packet_id == 0U) ||
        !AppendUtf8(output, capacity, &position, topic) ||
        (qos == 1U && !AppendUint16(output, capacity, &position, packet_id)) ||
        payload_length > capacity - position) {
        return 0U;
    }
    memcpy(output + position, payload, payload_length);
    position += payload_length;
    header = 0x30U | (duplicate ? 0x08U : 0U) | (uint8_t)(qos << 1) |
             (retain ? 0x01U : 0U);
    return FinishPacket(header, output, capacity, 5U, position);
}

size_t MqttWire_BuildPubAck(uint16_t packet_id, uint8_t *output, size_t capacity)
{
    if (output == NULL || capacity < 4U || packet_id == 0U) {
        return 0U;
    }
    output[0] = 0x40U;
    output[1] = 0x02U;
    output[2] = (uint8_t)(packet_id >> 8);
    output[3] = (uint8_t)packet_id;
    return 4U;
}

size_t MqttWire_BuildPingReq(uint8_t *output, size_t capacity)
{
    if (output == NULL || capacity < 2U) {
        return 0U;
    }
    output[0] = 0xC0U;
    output[1] = 0U;
    return 2U;
}
