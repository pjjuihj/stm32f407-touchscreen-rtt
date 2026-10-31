#ifndef IOT_MQTT_JSON_H
#define IOT_MQTT_JSON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MQTT_JSON_ID_LENGTH 16U
#define MQTT_JSON_DEVICE_ID_CAPACITY 32U
#define MQTT_JSON_COMMAND_CAPACITY 24U

typedef enum {
    MQTT_COMMAND_LED_SET = 0,
    MQTT_COMMAND_BUZZER_PULSE,
    MQTT_COMMAND_SYSTEM_GET,
    MQTT_COMMAND_UNKNOWN
} MqttCommandType;

typedef struct {
    uint32_t schema_version;
    char device_id[MQTT_JSON_DEVICE_ID_CAPACITY];
    char session_id[MQTT_JSON_ID_LENGTH + 1U];
    char command_id[MQTT_JSON_ID_LENGTH + 1U];
    char command_name[MQTT_JSON_COMMAND_CAPACITY];
    MqttCommandType type;
    uint8_t led_index;
    uint8_t led_on;
    uint16_t duration_ms;
} MqttCommand;

typedef enum {
    MQTT_COMMAND_PARSE_OK = 0,
    MQTT_COMMAND_PARSE_INVALID,
    MQTT_COMMAND_PARSE_UNSUPPORTED_VERSION
} MqttCommandParseResult;

typedef struct {
    const char *station_state;
    const char *ip_address;
    uint32_t uptime_s;
    uint8_t bluetooth_connected;
    uint8_t led[3];
    uint8_t buzzer_gpio_high;
    uint32_t dropped_snapshots;
    uint8_t remote_control_available;
} MqttDeviceState;

typedef struct {
    uint8_t led[3];
    uint8_t buzzer_gpio_high;
} MqttActuatorReadback;

MqttCommandParseResult MqttJson_ParseCommand(const uint8_t *json, size_t length,
                                             MqttCommand *command);
size_t MqttJson_BuildState(const MqttDeviceState *state, const char *device_id,
                           const char *session_id, uint32_t sequence,
                           char *output, size_t capacity);
size_t MqttJson_BuildAck(const MqttCommand *command, const char *device_id,
                         const char *session_id, const char *result,
                         const MqttActuatorReadback *readback,
                         char *output, size_t capacity);

#endif
