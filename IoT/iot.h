#ifndef IOT_H
#define IOT_H

#include "bluetooth_command_parser.h"

#include <stdint.h>

typedef struct {
    uint8_t led_state[3];
    uint8_t buzzer_state;
    uint8_t wifi_connected;
    uint8_t bt_connected;
    uint32_t uptime;
    char device_id[32];
} IoT_Status;

typedef uint8_t IoT_Module;
enum {
    IOT_MODULE_LED = 0,
    IOT_MODULE_BUZZER,
    IOT_MODULE_SYSTEM,
    IOT_MODULE_MAX
};

typedef uint8_t IoT_Action;
enum {
    IOT_ACTION_SET = 0,
    IOT_ACTION_GET,
    IOT_ACTION_TOGGLE,
    IOT_ACTION_MAX
};

typedef struct {
    IoT_Module module;
    IoT_Action action;
    uint32_t param;
} IoT_Command;

typedef uint8_t IoT_Result;
enum {
    IOT_RESULT_OK = 0,
    IOT_RESULT_ERROR,
    IOT_RESULT_INVALID_CMD,
    IOT_RESULT_INVALID_PARAM
};

void IoT_Init(const char *device_id);
void IoT_Process(void);
IoT_Status *IoT_GetStatus(void);
IoT_Result IoT_HandleCommand(const IoT_Command *command);
IoT_Result IoT_HandleBluetoothCommand(BluetoothCommand command);
void IoT_SendResponse(const char *response);

#endif
