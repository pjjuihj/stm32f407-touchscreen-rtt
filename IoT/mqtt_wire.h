#ifndef IOT_MQTT_WIRE_H
#define IOT_MQTT_WIRE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MQTT_WIRE_RX_CAPACITY 1024U
#define MQTT_WIRE_PACKET_CAPACITY 768U

typedef struct {
    uint8_t bytes[MQTT_WIRE_RX_CAPACITY];
    uint16_t length;
} MqttWireParser;

typedef void (*MqttWirePacketFn)(void *context, const uint8_t *packet,
                                 size_t packet_length);

void MqttWire_Init(MqttWireParser *parser);
bool MqttWire_FeedByte(MqttWireParser *parser, uint8_t byte,
                       MqttWirePacketFn packet_fn, void *context);
size_t MqttWire_BuildConnect(const char *client_id, const char *will_topic,
                             const char *will_payload, uint16_t keep_alive_s,
                             uint8_t *output, size_t capacity);
size_t MqttWire_BuildSubscribe(uint16_t packet_id, const char *topic,
                               uint8_t qos, uint8_t *output, size_t capacity);
size_t MqttWire_BuildPublish(const char *topic, const uint8_t *payload,
                             size_t payload_length, uint8_t qos, bool retain,
                             bool duplicate, uint16_t packet_id,
                             uint8_t *output, size_t capacity);
size_t MqttWire_BuildPubAck(uint16_t packet_id, uint8_t *output, size_t capacity);
size_t MqttWire_BuildPingReq(uint8_t *output, size_t capacity);

#endif
