#ifndef TEST_GUI_FAKE_LVGL_H
#define TEST_GUI_FAKE_LVGL_H

#include <stdint.h>

typedef struct lv_display_t { uint8_t unused; } lv_display_t;
typedef struct lv_indev_t { uint8_t unused; } lv_indev_t;
typedef uint16_t lv_color_t;
typedef int16_t lv_coord_t;
typedef int lv_log_level_t;
typedef enum { LV_DISPLAY_RENDER_MODE_PARTIAL = 0 } lv_display_render_mode_t;
typedef enum { LV_INDEV_TYPE_POINTER = 0 } lv_indev_type_t;
typedef enum { LV_INDEV_STATE_RELEASED = 0, LV_INDEV_STATE_PRESSED = 1 } lv_indev_state_t;
typedef struct { lv_coord_t x; lv_coord_t y; } lv_point_t;
typedef struct { lv_indev_state_t state; lv_point_t point; } lv_indev_data_t;
typedef struct { int16_t x1; int16_t y1; int16_t x2; int16_t y2; } lv_area_t;

typedef void (*lv_display_flush_cb_t)(lv_display_t *, const lv_area_t *, uint8_t *);
typedef void (*lv_indev_read_cb_t)(lv_indev_t *, lv_indev_data_t *);
typedef void (*lv_log_print_g_cb_t)(lv_log_level_t, const char *);

#define LV_USE_LOG 1
#define LV_OPA_COVER 255U

lv_display_t *lv_display_create(int32_t horizontal, int32_t vertical);
void lv_display_set_flush_cb(lv_display_t *display, lv_display_flush_cb_t callback);
void lv_display_set_buffers(lv_display_t *display, void *buffer_a, void *buffer_b,
                            uint32_t buffer_size, lv_display_render_mode_t mode);
void lv_display_flush_ready(lv_display_t *display);
lv_indev_t *lv_indev_create(void);
void lv_indev_set_type(lv_indev_t *input, lv_indev_type_t type);
void lv_indev_set_read_cb(lv_indev_t *input, lv_indev_read_cb_t callback);
lv_display_t *lv_display_get_default(void);
void lv_indev_set_display(lv_indev_t *input, lv_display_t *display);
void lv_tick_set_cb(uint32_t (*callback)(void));
void lv_log_register_print_cb(lv_log_print_g_cb_t callback);

#endif
