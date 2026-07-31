/**
 * @file SEGGER_RTT_Conf.h
 * @brief SEGGER RTT configuration for STM32F407ZG project
 *
 * Configuration:
 * - 2 up channels: Channel 0 (LVGL logs, 1024 bytes), Channel 1 (App logs, 512 bytes)
 * - 2 down channels: For future use (command input)
 * - Critical sections use __disable_irq()/__enable_irq() (Cortex-M4)
 * - Non-blocking mode: trim data when buffer is full
 */

#ifndef SEGGER_RTT_CONF_H
#define SEGGER_RTT_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
 *
 *       Defines, configurable
 *
 **********************************************************************
 */

/**
 * Number of up-buffers (target -> host) available.
 * Channel 0: LVGL logs (1024 bytes)
 * Channel 1: App logs (512 bytes)
 */
#define SEGGER_RTT_MAX_NUM_UP_BUFFERS       2

/**
 * Number of down-buffers (host -> target) available.
 * Reserved for future use (command input).
 */
#define SEGGER_RTT_MAX_NUM_DOWN_BUFFERS     2

/**
 * Default size for up-buffer 0 (LVGL logs).
 * Can be overridden per-channel in SEGGER_RTT_Init().
 */
#define SEGGER_RTT_BUFFER_SIZE_UP           1024u

/**
 * Default size for down-buffer 0.
 * Small size as we don't expect host-to-target data.
 */
#define SEGGER_RTT_BUFFER_SIZE_DOWN         16u

/**
 * Mode for up-buffer 0.
 * SEGGER_RTT_MODE_NO_BLOCK_TRIM: Trim data to fit if buffer is full (safest for callbacks)
 */
#define SEGGER_RTT_MODE_DEFAULT             SEGGER_RTT_MODE_NO_BLOCK_TRIM

/*********************************************************************
 *
 *       RTT lock/unlock macros
 *
 *       Use Cortex-M4 interrupt disable/enable for thread safety.
 *       This is safe for both main loop and interrupt context.
 *
 **********************************************************************
 */
#include "cmsis_compiler.h"

#define SEGGER_RTT_LOCK()                   \
    do {                                     \
        unsigned long _primask = __get_PRIMASK(); \
        __disable_irq()

#define SEGGER_RTT_UNLOCK()                     \
        __set_PRIMASK(_primask);                \
    } while (0)

/*********************************************************************
 *
 *       Optimizations
 *
 **********************************************************************
 */

/**
 * Use memcpy for buffer operations when available.
 * ARMCC has optimized memcpy for Cortex-M4.
 */
#define SEGGER_RTT_MEMCPY_USE_BYTELOOP      0

/**
 * Allow section placement for RTT control block.
 * Placed in .rtt_block section for easy identification.
 */
#define SEGGER_RTT_SECTION

#ifdef __cplusplus
}
#endif

#endif /* SEGGER_RTT_CONF_H */
