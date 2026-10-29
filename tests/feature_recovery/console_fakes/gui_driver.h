#ifndef TEST_CONSOLE_GUI_DRIVER_H
#define TEST_CONSOLE_GUI_DRIVER_H

#include <stdint.h>

typedef struct {
    uint32_t loop_completed, flush_started, flush_completed;
    uint32_t touch_started, touch_completed;
    uint32_t last_loop_tick, last_flush_tick, last_touch_tick;
    uint32_t last_flush_pixels;
    int16_t last_flush_x1, last_flush_y1, last_flush_x2, last_flush_y2;
} gui_diag_snapshot_t;

void gui_diag_get_snapshot(gui_diag_snapshot_t *snapshot);

typedef enum {
    LOG_OUTPUT_RTT = 0,
    LOG_OUTPUT_UART
} LogOutputMode;

void gui_log_set_output_mode(LogOutputMode mode);

#endif
