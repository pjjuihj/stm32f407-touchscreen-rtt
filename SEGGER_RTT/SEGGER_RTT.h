/**
 * @file SEGGER_RTT.h
 * @brief Public header for SEGGER RTT library
 *
 * This is a convenience header that includes the core RTT header.
 * Include this file in your application code to use RTT functions.
 *
 * Usage:
 *   #include "SEGGER_RTT.h"
 *
 *   // Initialize RTT (once, early in main)
 *   SEGGER_RTT_Init();
 *
 *   // Write logs
 *   SEGGER_RTT_WriteString(0, "Hello from LVGL!\n");
 *   SEGGER_RTT_WriteString(1, "Hello from App!\n");
 *
 * Channel assignments:
 *   Channel 0: LVGL logs (1024 bytes buffer)
 *   Channel 1: App logs (512 bytes buffer)
 */

#ifndef SEGGER_RTT_PUBLIC_H
#define SEGGER_RTT_PUBLIC_H

#include "RTT/SEGGER_RTT.h"

/**
 * @brief Channel index definitions for readability
 */
#define RTT_CHANNEL_LVGL    0   /* LVGL log channel */
#define RTT_CHANNEL_APP     1   /* Application log channel */

#endif /* SEGGER_RTT_PUBLIC_H */
