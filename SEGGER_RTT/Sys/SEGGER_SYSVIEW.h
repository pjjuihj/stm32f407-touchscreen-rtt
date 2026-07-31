/**
 * @file SEGGER_SYSVIEW.h
 * @brief SEGGER SystemView interface for real-time analysis
 *
 * Minimal SystemView implementation for STM32F407ZG.
 * SystemView provides real-time recording and visualization of
 * OS-aware application events.
 */

#ifndef SEGGER_SYSVIEW_H
#define SEGGER_SYSVIEW_H

#include "SEGGER_RTT.h"
#include <stdint.h>

/*===========================================================================
 * Configuration
 *===========================================================================*/

#define SYSVIEW_USE_RTT          1
#define SYSVIEW_RTT_CHANNEL      2  /* Use channel 2 for SystemView */

/* SystemView command IDs */
#define SYSVIEW_CMD_ID_START     1
#define SYSVIEW_CMD_ID_STOP      2
#define SYSVIEW_CMD_ID_OVERFLOW  3

/* SystemView event IDs */
#define SYSVIEW_EVT_ID_PRINT     1
#define SYSVIEW_EVT_ID_TASK_START 2
#define SYSVIEW_EVT_ID_TASK_STOP  3
#define SYSVIEW_EVT_ID_TASK_PRIO  4
#define SYSVIEW_EVT_ID_IRQ_ENTER  5
#define SYSVIEW_EVT_ID_IRQ_LEAVE  6
#define SYSVIEW_EVT_ID_INIT      7

/*===========================================================================
 * API Functions
 *===========================================================================*/

/**
 * @brief Initialize SystemView
 */
void SEGGER_SYSVIEW_Init(void);

/**
 * @brief Start SystemView recording
 */
void SEGGER_SYSVIEW_Start(void);

/**
 * @brief Stop SystemView recording
 */
void SEGGER_SYSVIEW_Stop(void);

/**
 * @brief Send a print event
 * @param s String to print
 */
void SEGGER_SYSVIEW_Print(const char *s);

/**
 * @brief Send an IRQ enter event
 * @param IRQNum IRQ number
 */
void SEGGER_SYSVIEW_OnIRQEnter(unsigned int IRQNum);

/**
 * @brief Send an IRQ leave event
 * @param IRQNum IRQ number
 */
void SEGGER_SYSVIEW_OnIRQLeave(unsigned int IRQNum);

/**
 * @brief Send a task start event
 * @param TaskID Task ID
 * @param TaskName Task name
 * @param Priority Task priority
 */
void SEGGER_SYSVIEW_OnTaskStart(unsigned int TaskID, const char *TaskName, unsigned int Priority);

/**
 * @brief Send a task stop event
 * @param TaskID Task ID
 */
void SEGGER_SYSVIEW_OnTaskStop(unsigned int TaskID);

/**
 * @brief Send a task priority change event
 * @param TaskID Task ID
 * @param NewPriority New priority
 */
void SEGGER_SYSVIEW_OnTaskPriorityChange(unsigned int TaskID, unsigned int NewPriority);

#endif /* SEGGER_SYSVIEW_H */
