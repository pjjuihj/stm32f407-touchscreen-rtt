#include "mqtt_json.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    const uint8_t *bytes;
    size_t length;
    size_t position;
} JsonCursor;

static void SkipWhitespace(JsonCursor *cursor)
{
    while (cursor->position < cursor->length) {
        uint8_t byte = cursor->bytes[cursor->position];
        if (byte != ' ' && byte != '\t' && byte != '\r' && byte != '\n') {
            break;
        }
        cursor->position++;
    }
}

static bool ParseString(JsonCursor *cursor, char *output, size_t capacity)
{
    size_t start;
    size_t length;

    SkipWhitespace(cursor);
    if (cursor->position >= cursor->length || cursor->bytes[cursor->position] != '"') {
        return false;
    }
    start = ++cursor->position;
    while (cursor->position < cursor->length &&
           cursor->bytes[cursor->position] != '"') {
        uint8_t byte = cursor->bytes[cursor->position];
        if (byte < 0x20U || byte == '\\') {
            return false;
        }
        cursor->position++;
    }
    if (cursor->position >= cursor->length) {
        return false;
    }
    length = cursor->position - start;
    if (length + 1U > capacity) {
        return false;
    }
    memcpy(output, cursor->bytes + start, length);
    output[length] = '\0';
    cursor->position++;
    return true;
}

static bool ParseUint32(JsonCursor *cursor, uint32_t *value)
{
    uint32_t result = 0U;
    size_t digits = 0U;

    SkipWhitespace(cursor);
    while (cursor->position < cursor->length) {
        uint8_t byte = cursor->bytes[cursor->position];
        uint32_t digit;
        if (byte < '0' || byte > '9') {
            break;
        }
        digit = (uint32_t)(byte - '0');
        if (result > (UINT32_MAX - digit) / 10U) {
            return false;
        }
        result = result * 10U + digit;
        cursor->position++;
        digits++;
    }
    if (digits == 0U) {
        return false;
    }
    *value = result;
    return true;
}

static bool ParseHexId(char *value)
{
    size_t index;
    if (strlen(value) != MQTT_JSON_ID_LENGTH) {
        return false;
    }
    for (index = 0U; index < MQTT_JSON_ID_LENGTH; index++) {
        char byte = value[index];
        if (byte >= 'A' && byte <= 'F') {
            value[index] = (char)(byte - 'A' + 'a');
        } else if (!((byte >= '0' && byte <= '9') ||
                     (byte >= 'a' && byte <= 'f'))) {
            return false;
        }
    }
    return true;
}

static bool ParseMemberValue(JsonCursor *cursor, MqttCommand *command,
                             const char *key, uint16_t *seen)
{
    char text[MQTT_JSON_DEVICE_ID_CAPACITY];
    uint32_t number;
    uint16_t bit;

    if (strcmp(key, "schema_version") == 0) {
        bit = 1U << 0;
        if (!ParseUint32(cursor, &number)) return false;
        command->schema_version = number;
    } else if (strcmp(key, "device_id") == 0) {
        bit = 1U << 1;
        if (!ParseString(cursor, command->device_id, sizeof(command->device_id))) return false;
    } else if (strcmp(key, "session_id") == 0) {
        bit = 1U << 2;
        if (!ParseString(cursor, command->session_id, sizeof(command->session_id)) ||
            !ParseHexId(command->session_id)) return false;
    } else if (strcmp(key, "command_id") == 0) {
        bit = 1U << 3;
        if (!ParseString(cursor, command->command_id, sizeof(command->command_id)) ||
            !ParseHexId(command->command_id)) return false;
    } else if (strcmp(key, "command") == 0) {
        bit = 1U << 4;
        if (!ParseString(cursor, command->command_name,
                         sizeof(command->command_name))) return false;
    } else if (strcmp(key, "led_index") == 0) {
        bit = 1U << 5;
        if (!ParseUint32(cursor, &number) || number > UINT8_MAX) return false;
        command->led_index = (uint8_t)number;
    } else if (strcmp(key, "value") == 0) {
        bit = 1U << 6;
        if (!ParseString(cursor, text, sizeof(text))) return false;
        if (strcmp(text, "on") == 0) command->led_on = 1U;
        else if (strcmp(text, "off") == 0) command->led_on = 0U;
        else return false;
    } else if (strcmp(key, "duration_ms") == 0) {
        bit = 1U << 7;
        if (!ParseUint32(cursor, &number) || number > UINT16_MAX) return false;
        command->duration_ms = (uint16_t)number;
    } else {
        return false;
    }

    if ((*seen & bit) != 0U) {
        return false;
    }
    *seen |= bit;
    return true;
}

MqttCommandParseResult MqttJson_ParseCommand(const uint8_t *json, size_t length,
                                             MqttCommand *command)
{
    JsonCursor cursor;
    uint16_t seen = 0U;

    if (json == NULL || command == NULL || length == 0U || length > 384U) {
        return MQTT_COMMAND_PARSE_INVALID;
    }
    memset(command, 0, sizeof(*command));
    cursor.bytes = json;
    cursor.length = length;
    cursor.position = 0U;
    SkipWhitespace(&cursor);
    if (cursor.position >= cursor.length || cursor.bytes[cursor.position++] != '{') {
        return MQTT_COMMAND_PARSE_INVALID;
    }
    for (;;) {
        char key[32];
        SkipWhitespace(&cursor);
        if (cursor.position < cursor.length && cursor.bytes[cursor.position] == '}') {
            cursor.position++;
            break;
        }
        if (!ParseString(&cursor, key, sizeof(key))) {
            return MQTT_COMMAND_PARSE_INVALID;
        }
        SkipWhitespace(&cursor);
        if (cursor.position >= cursor.length || cursor.bytes[cursor.position++] != ':') {
            return MQTT_COMMAND_PARSE_INVALID;
        }
        if (!ParseMemberValue(&cursor, command, key, &seen)) {
            return MQTT_COMMAND_PARSE_INVALID;
        }
        SkipWhitespace(&cursor);
        if (cursor.position < cursor.length && cursor.bytes[cursor.position] == ',') {
            cursor.position++;
            continue;
        }
        if (cursor.position < cursor.length && cursor.bytes[cursor.position] == '}') {
            cursor.position++;
            break;
        }
        return MQTT_COMMAND_PARSE_INVALID;
    }
    SkipWhitespace(&cursor);
    if (cursor.position != cursor.length || (seen & 0x001FU) != 0x001FU) {
        return MQTT_COMMAND_PARSE_INVALID;
    }
    if (command->schema_version != 1U) {
        return MQTT_COMMAND_PARSE_UNSUPPORTED_VERSION;
    }

    if (strcmp(command->command_name, "led.set") == 0) {
        if ((seen & ((1U << 5) | (1U << 6))) != ((1U << 5) | (1U << 6))) {
            return MQTT_COMMAND_PARSE_INVALID;
        }
        if ((seen & (1U << 7)) != 0U) {
            return MQTT_COMMAND_PARSE_INVALID;
        }
        if (command->led_index > 2U) {
            command->type = MQTT_COMMAND_LED_SET;
            return MQTT_COMMAND_PARSE_OK;
        }
        command->type = MQTT_COMMAND_LED_SET;
    } else if (strcmp(command->command_name, "buzzer.pulse") == 0) {
        if ((seen & ((1U << 5) | (1U << 6))) != 0U) {
            return MQTT_COMMAND_PARSE_INVALID;
        }
        command->type = MQTT_COMMAND_BUZZER_PULSE;
        if ((seen & (1U << 7)) == 0U) {
            command->duration_ms = 500U;
        } else if (command->duration_ms < 100U || command->duration_ms > 1000U) {
            return MQTT_COMMAND_PARSE_INVALID;
        }
    } else if (strcmp(command->command_name, "system.get") == 0) {
        if ((seen & 0x00E0U) != 0U) {
            return MQTT_COMMAND_PARSE_INVALID;
        }
        command->type = MQTT_COMMAND_SYSTEM_GET;
    } else {
        command->type = MQTT_COMMAND_UNKNOWN;
    }
    return MQTT_COMMAND_PARSE_OK;
}

size_t MqttJson_BuildState(const MqttDeviceState *state, const char *device_id,
                           const char *session_id, uint32_t sequence,
                           char *output, size_t capacity)
{
    char ip_json[20];
    int length;
    if (state == NULL || device_id == NULL || session_id == NULL ||
        output == NULL || capacity == 0U) {
        return 0U;
    }
    if (state->ip_address == NULL || state->ip_address[0] == '\0') {
        (void)strncpy(ip_json, "null", sizeof(ip_json));
    } else {
        length = snprintf(ip_json, sizeof(ip_json), "\"%s\"", state->ip_address);
        if (length < 0 || (size_t)length >= sizeof(ip_json)) {
            return 0U;
        }
    }
    length = snprintf(output, capacity,
        "{\"schema_version\":1,\"device_id\":\"%s\",\"session_id\":\"%s\","
        "\"seq\":%lu,\"uptime_s\":%lu,\"wifi\":{\"sta\":\"%s\",\"ip\":%s},"
        "\"bt_connected\":%s,\"led\":[%s,%s,%s],\"buzzer_gpio_high\":%s,"
        "\"dropped_snapshots\":%lu,\"remote_control_available\":%s}",
        device_id, session_id, (unsigned long)sequence,
        (unsigned long)state->uptime_s,
        state->station_state != NULL ? state->station_state : "unknown",
        ip_json,
        state->bluetooth_connected != 0U ? "true" : "false",
        state->led[0] != 0U ? "true" : "false",
        state->led[1] != 0U ? "true" : "false",
        state->led[2] != 0U ? "true" : "false",
        state->buzzer_gpio_high != 0U ? "true" : "false",
        (unsigned long)state->dropped_snapshots,
        state->remote_control_available != 0U ? "true" : "false");
    if (length < 0 || (size_t)length >= capacity) {
        return 0U;
    }
    return (size_t)length;
}

size_t MqttJson_BuildAck(const MqttCommand *command, const char *device_id,
                         const char *session_id, const char *result,
                         const MqttActuatorReadback *readback,
                         char *output, size_t capacity)
{
    int length;
    if (command == NULL || device_id == NULL || session_id == NULL ||
        result == NULL || readback == NULL || output == NULL || capacity == 0U) {
        return 0U;
    }
    length = snprintf(output, capacity,
        "{\"schema_version\":1,\"device_id\":\"%s\",\"session_id\":\"%s\","
        "\"command_id\":\"%s\",\"command\":\"%s\",\"result\":\"%s\","
        "\"readback\":{\"led\":[%s,%s,%s],\"buzzer_gpio_high\":%s}}",
        device_id, session_id, command->command_id,
        command->command_name[0] != '\0' ? command->command_name : "unknown",
        result,
        readback->led[0] != 0U ? "true" : "false",
        readback->led[1] != 0U ? "true" : "false",
        readback->led[2] != 0U ? "true" : "false",
        readback->buzzer_gpio_high != 0U ? "true" : "false");
    if (length < 0 || (size_t)length >= capacity) {
        return 0U;
    }
    return (size_t)length;
}
