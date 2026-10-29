#ifndef TEST_GUI_FAKE_LCD_H
#define TEST_GUI_FAKE_LCD_H

#include <stdint.h>

typedef uint16_t u16;
extern volatile u16 LCD_DATA;
void LCD_Open_Window(int16_t x, int16_t y, u16 width, u16 height);
void LCD_WriteGRAM(void);

#endif
