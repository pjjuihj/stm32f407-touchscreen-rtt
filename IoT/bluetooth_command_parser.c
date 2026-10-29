#include "bluetooth_command_parser.h"

#include <string.h>

typedef struct {
    const char *text;
    BluetoothCommand command;
} CommandEntry;

static const CommandEntry g_commands[] = {
    {"open_led1", BT_COMMAND_LED1_ON},
    {"close_led1", BT_COMMAND_LED1_OFF},
    {"open_led2", BT_COMMAND_LED2_ON},
    {"close_led2", BT_COMMAND_LED2_OFF},
    {"open_led3", BT_COMMAND_LED3_ON},
    {"close_led3", BT_COMMAND_LED3_OFF},
    {"open_beep", BT_COMMAND_BUZZER_ON},
    {"close_beep", BT_COMMAND_BUZZER_OFF}
};

void BluetoothCommandParser_Init(BluetoothCommandParser *parser)
{
    if (parser != NULL) {
        memset(parser, 0, sizeof(*parser));
    }
}

void BluetoothCommandParser_Reset(BluetoothCommandParser *parser)
{
    BluetoothCommandParser_Init(parser);
}

static BluetoothCommand Decode(const char *frame, uint8_t length)
{
    size_t index;
    if (frame == NULL) {
        return BT_COMMAND_NONE;
    }
    for (index = 0U; index < sizeof(g_commands) / sizeof(g_commands[0]); index++) {
        size_t expected = strlen(g_commands[index].text);
        if (expected == length && memcmp(frame, g_commands[index].text, expected) == 0) {
            return g_commands[index].command;
        }
    }
    return BT_COMMAND_NONE;
}

bool BluetoothCommandParser_Feed(BluetoothCommandParser *parser, uint8_t byte,
                                 BluetoothCommand *command)
{
    BluetoothCommand decoded = BT_COMMAND_NONE;

    if (command != NULL) {
        *command = BT_COMMAND_NONE;
    }
    if (parser == NULL) {
        return false;
    }
    if (byte == (uint8_t)'S') {
        parser->length = 0U;
        parser->overflow = 0U;
        parser->collecting = 1U;
        parser->frame[0] = '\0';
        return false;
    }
    if (parser->collecting == 0U) {
        return false;
    }
    if (byte == (uint8_t)'E') {
        if (parser->overflow == 0U) {
            decoded = Decode(parser->frame, parser->length);
        }
        parser->length = 0U;
        parser->overflow = 0U;
        parser->collecting = 0U;
        parser->frame[0] = '\0';
        if (command != NULL) {
            *command = decoded;
        }
        return decoded != BT_COMMAND_NONE;
    }
    if (parser->overflow != 0U) {
        return false;
    }
    if (parser->length >= BT_COMMAND_FRAME_CAPACITY) {
        parser->overflow = 1U;
        return false;
    }
    parser->frame[parser->length++] = (char)byte;
    parser->frame[parser->length] = '\0';
    return false;
}
