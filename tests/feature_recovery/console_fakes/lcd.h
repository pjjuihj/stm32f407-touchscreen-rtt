#ifndef TEST_CONSOLE_LCD_H
#define TEST_CONSOLE_LCD_H

void LCD_Init(void);
void *lv_screen_active(void);
void lv_obj_invalidate(void *object);
void lv_refr_now(void *display);

#endif
