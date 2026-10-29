#include "iot.h"
#include "bluetooth_command_parser.h"
#include "board_io.h"
#include "wifi.h"
#include "bluetooth.h"

#include <assert.h>
#include <string.h>

GPIO_TypeDef test_gpiof;
GPIO_TypeDef test_uart4_instance;
GPIO_TypeDef test_usart3_instance;

static bool g_leds[3];
static bool g_buzzer;
static unsigned int g_uart_replies;

void LED_Set(uint8_t led, bool enabled) { assert(led < 3U); g_leds[led] = enabled; }
bool LED_Get(uint8_t led) { assert(led < 3U); return g_leds[led]; }
void BEEP_Set(bool enabled) { g_buzzer = enabled; }
bool BEEP_Get(void) { return g_buzzer; }
uint32_t HAL_GetTick(void) { return 0U; }
void BT_Init(void) {}
bool BT_Connect(void) { return false; }
void BT_Disconnect(void) {}
bool BT_IsConnected(void) { return false; }
void BT_Process(void) {}
void BT_SetRxCallback(BT_RxCallback callback) { (void)callback; }
void WiFi_Init(void) {}
void WiFi_Process(void) {}
bool WiFi_IsConnected(void) { return false; }
void USART1_SendReply(const char *text) { (void)text; g_uart_replies++; }

int main(void)
{
    IoT_Command command;
    IoT_Status *status;
    IoT_Init(NULL);
    status = IoT_GetStatus();
    assert(IoT_HandleBluetoothCommand(BT_COMMAND_LED1_ON) == IOT_RESULT_OK);
    assert(IoT_HandleBluetoothCommand(BT_COMMAND_LED2_ON) == IOT_RESULT_OK);
    assert(IoT_HandleBluetoothCommand(BT_COMMAND_LED3_ON) == IOT_RESULT_OK);
    assert(g_leds[0] && g_leds[1] && g_leds[2]);
    assert(status->led_state[0] && status->led_state[1] && status->led_state[2]);
    assert(IoT_HandleBluetoothCommand(BT_COMMAND_LED2_OFF) == IOT_RESULT_OK);
    assert(!g_leds[1] && status->led_state[1] == 0U);
    assert(IoT_HandleBluetoothCommand(BT_COMMAND_BUZZER_ON) == IOT_RESULT_OK);
    assert(g_buzzer && status->buzzer_state == 1U);
    assert(IoT_HandleBluetoothCommand(BT_COMMAND_BUZZER_OFF) == IOT_RESULT_OK);
    assert(!g_buzzer && status->buzzer_state == 0U);
    assert(g_uart_replies == 0U);

    command.module = IOT_MODULE_LED;
    command.action = IOT_ACTION_SET;
    command.param = 0U;
    assert(IoT_HandleCommand(&command) == IOT_RESULT_OK);
    assert(g_uart_replies == 1U);
    return 0;
}
