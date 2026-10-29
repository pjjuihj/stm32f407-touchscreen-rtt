#ifndef BLUETOOTH_COMMAND_PARSER_H
#define BLUETOOTH_COMMAND_PARSER_H

#include <stdbool.h>
#include <stdint.h>

#define BT_COMMAND_FRAME_CAPACITY 24U

typedef enum {
    BT_COMMAND_NONE = 0,
    BT_COMMAND_LED1_ON,
    BT_COMMAND_LED1_OFF,
    BT_COMMAND_LED2_ON,
    BT_COMMAND_LED2_OFF,
    BT_COMMAND_LED3_ON,
    BT_COMMAND_LED3_OFF,
    BT_COMMAND_BUZZER_ON,
    BT_COMMAND_BUZZER_OFF
} BluetoothCommand;

typedef struct {
    char frame[BT_COMMAND_FRAME_CAPACITY + 1U];
    uint8_t length;
    uint8_t collecting;
    uint8_t overflow;
} BluetoothCommandParser;

void BluetoothCommandParser_Init(BluetoothCommandParser *parser);
void BluetoothCommandParser_Reset(BluetoothCommandParser *parser);
bool BluetoothCommandParser_Feed(BluetoothCommandParser *parser, uint8_t byte,
                                 BluetoothCommand *command);

#endif
