#ifndef IOT_MQTT_CLIENT_H
#define IOT_MQTT_CLIENT_H

#include "mqtt_json.h"

#include <stdbool.h>

typedef const char *(*MqttCommandHandler)(const MqttCommand *command,
                                          MqttActuatorReadback *readback,
                                          void *context);
typedef void (*MqttReadbackHandler)(MqttActuatorReadback *readback,
                                    void *context);

void MqttClient_Init(const char *device_id, const char *session_id,
                     bool remote_control_available,
                     MqttCommandHandler command_handler,
                     MqttReadbackHandler readback_handler,
                     void *context);
void MqttClient_Process(const MqttDeviceState *state);
bool MqttClient_IsOnline(void);

#endif
