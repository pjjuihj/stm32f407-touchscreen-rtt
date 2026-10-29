#include "bluetooth_command_parser.h"

#include <assert.h>
#include <stddef.h>

static BluetoothCommand Feed(BluetoothCommandParser *parser, const char *frame)
{
    BluetoothCommand result = BT_COMMAND_NONE;
    size_t index;
    for (index = 0U; frame[index] != '\0'; index++) {
        BluetoothCommand current = BT_COMMAND_NONE;
        if (BluetoothCommandParser_Feed(parser, (uint8_t)frame[index], &current)) {
            result = current;
        }
    }
    return result;
}

int main(void)
{
    BluetoothCommandParser parser;
    BluetoothCommand command;
    size_t index;

    BluetoothCommandParser_Init(&parser);
    assert(Feed(&parser, "noiseSopen_led1") == BT_COMMAND_NONE);
    assert(Feed(&parser, "E") == BT_COMMAND_LED1_ON);
    assert(Feed(&parser, "Sclose_led2E") == BT_COMMAND_LED2_OFF);
    assert(Feed(&parser, "Sopen_led3E") == BT_COMMAND_LED3_ON);
    assert(Feed(&parser, "Sclose_led1E") == BT_COMMAND_LED1_OFF);
    assert(Feed(&parser, "Sopen_beepE") == BT_COMMAND_BUZZER_ON);
    assert(Feed(&parser, "Sclose_beepE") == BT_COMMAND_BUZZER_OFF);
    assert(Feed(&parser, "Sapp_mode1E") == BT_COMMAND_NONE);
    assert(Feed(&parser, "Sopen_led1Sclose_led3E") == BT_COMMAND_LED3_OFF);

    BluetoothCommandParser_Init(&parser);
    assert(!BluetoothCommandParser_Feed(&parser, (uint8_t)'S', &command));
    for (index = 0U; index < BT_COMMAND_FRAME_CAPACITY + 4U; index++) {
        assert(!BluetoothCommandParser_Feed(&parser, (uint8_t)'x', &command));
    }
    assert(!BluetoothCommandParser_Feed(&parser, (uint8_t)'E', &command));
    assert(Feed(&parser, "Sopen_beepE") == BT_COMMAND_BUZZER_ON);
    assert(!BluetoothCommandParser_Feed(&parser, (uint8_t)'E', NULL));
    return 0;
}
