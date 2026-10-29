#ifndef IOT_BOARD_IO_H
#define IOT_BOARD_IO_H

#include <stdbool.h>
#include <stdint.h>

void LED_Set(uint8_t led, bool enabled);
bool LED_Get(uint8_t led);
void BEEP_Set(bool enabled);
bool BEEP_Get(void);

#endif
