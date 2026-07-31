/**
 * @file SEGGER_SYSVIEW.c
 * @brief SEGGER SystemView implementation for STM32F407ZG
 *
 * Minimal SystemView implementation using SEGGER RTT channel 2.
 * This provides basic event recording for SystemView software.
 */

#include "SEGGER_SYSVIEW.h"
#include <string.h>
#include <stdio.h>

/*===========================================================================
 * Private Data
 *===========================================================================*/

static int _IsStarted = 0;

/*===========================================================================
 * Private Functions
 *===========================================================================*/

/**
 * @brief Send a SystemView event
 * @param EventID Event ID
 * @param pData Data payload
 * @param Len Data length
 */
static void _SendEvent(unsigned char EventID, const void *pData, unsigned char Len)
{
    unsigned char Header[3];

    /* SystemView packet format:
     * Byte 0: Length (1 byte for short packets)
     * Byte 1: Event ID
     * Byte 2+: Data
     */
    Header[0] = 2 + Len;  /* Total length: ID + Data */
    Header[1] = EventID;

    SEGGER_RTT_Write(SYSVIEW_RTT_CHANNEL, Header, 2);
    if (pData && Len > 0) {
        SEGGER_RTT_Write(SYSVIEW_RTT_CHANNEL, pData, Len);
    }
}

/**
 * @brief Send a string event
 * @param EventID Event ID
 * @param s String to send
 */
static void _SendString(unsigned char EventID, const char *s)
{
    unsigned char Len;
    unsigned char Header[3];

    if (s == NULL) {
        s = "";
    }

    Len = (unsigned char)strlen(s);
    if (Len > 250) Len = 250;  /* Limit string length */

    Header[0] = 2 + Len + 1;  /* ID + Len + String + Null */
    Header[1] = EventID;

    SEGGER_RTT_Write(SYSVIEW_RTT_CHANNEL, Header, 2);
    SEGGER_RTT_Write(SYSVIEW_RTT_CHANNEL, s, Len);
    SEGGER_RTT_Write(SYSVIEW_RTT_CHANNEL, "\0", 1);
}

/*===========================================================================
 * Public Functions
 *===========================================================================*/

/**
 * @brief Initialize SystemView
 */
void SEGGER_SYSVIEW_Init(void)
{
    _IsStarted = 0;

    /* Send init event */
    unsigned char InitData[8] = {0};
    _SendEvent(SYSVIEW_EVT_ID_INIT, InitData, sizeof(InitData));
}

/**
 * @brief Start SystemView recording
 */
void SEGGER_SYSVIEW_Start(void)
{
    _IsStarted = 1;
    SEGGER_RTT_WriteString(SYSVIEW_RTT_CHANNEL, "SystemView started\n");
}

/**
 * @brief Stop SystemView recording
 */
void SEGGER_SYSVIEW_Stop(void)
{
    _IsStarted = 0;
    SEGGER_RTT_WriteString(SYSVIEW_RTT_CHANNEL, "SystemView stopped\n");
}

/**
 * @brief Send a print event
 * @param s String to print
 */
void SEGGER_SYSVIEW_Print(const char *s)
{
    if (!_IsStarted) return;
    _SendString(SYSVIEW_EVT_ID_PRINT, s);
}

/**
 * @brief Send an IRQ enter event
 * @param IRQNum IRQ number
 */
void SEGGER_SYSVIEW_OnIRQEnter(unsigned int IRQNum)
{
    if (!_IsStarted) return;

    unsigned char Data[4];
    Data[0] = (unsigned char)(IRQNum & 0xFF);
    Data[1] = (unsigned char)((IRQNum >> 8) & 0xFF);
    Data[2] = (unsigned char)((IRQNum >> 16) & 0xFF);
    Data[3] = (unsigned char)((IRQNum >> 24) & 0xFF);

    _SendEvent(SYSVIEW_EVT_ID_IRQ_ENTER, Data, 4);
}

/**
 * @brief Send an IRQ leave event
 * @param IRQNum IRQ number
 */
void SEGGER_SYSVIEW_OnIRQLeave(unsigned int IRQNum)
{
    if (!_IsStarted) return;

    unsigned char Data[4];
    Data[0] = (unsigned char)(IRQNum & 0xFF);
    Data[1] = (unsigned char)((IRQNum >> 8) & 0xFF);
    Data[2] = (unsigned char)((IRQNum >> 16) & 0xFF);
    Data[3] = (unsigned char)((IRQNum >> 24) & 0xFF);

    _SendEvent(SYSVIEW_EVT_ID_IRQ_LEAVE, Data, 4);
}

/**
 * @brief Send a task start event
 * @param TaskID Task ID
 * @param TaskName Task name
 * @param Priority Task priority
 */
void SEGGER_SYSVIEW_OnTaskStart(unsigned int TaskID, const char *TaskName, unsigned int Priority)
{
    if (!_IsStarted) return;

    unsigned char Data[8];
    Data[0] = (unsigned char)(TaskID & 0xFF);
    Data[1] = (unsigned char)((TaskID >> 8) & 0xFF);
    Data[2] = (unsigned char)((TaskID >> 16) & 0xFF);
    Data[3] = (unsigned char)((TaskID >> 24) & 0xFF);
    Data[4] = (unsigned char)(Priority & 0xFF);
    Data[5] = (unsigned char)((Priority >> 8) & 0xFF);
    Data[6] = (unsigned char)((Priority >> 16) & 0xFF);
    Data[7] = (unsigned char)((Priority >> 24) & 0xFF);

    _SendEvent(SYSVIEW_EVT_ID_TASK_START, Data, 8);

    /* Also send task name */
    _SendString(SYSVIEW_EVT_ID_PRINT, TaskName);
}

/**
 * @brief Send a task stop event
 * @param TaskID Task ID
 */
void SEGGER_SYSVIEW_OnTaskStop(unsigned int TaskID)
{
    if (!_IsStarted) return;

    unsigned char Data[4];
    Data[0] = (unsigned char)(TaskID & 0xFF);
    Data[1] = (unsigned char)((TaskID >> 8) & 0xFF);
    Data[2] = (unsigned char)((TaskID >> 16) & 0xFF);
    Data[3] = (unsigned char)((TaskID >> 24) & 0xFF);

    _SendEvent(SYSVIEW_EVT_ID_TASK_STOP, Data, 4);
}

/**
 * @brief Send a task priority change event
 * @param TaskID Task ID
 * @param NewPriority New priority
 */
void SEGGER_SYSVIEW_OnTaskPriorityChange(unsigned int TaskID, unsigned int NewPriority)
{
    if (!_IsStarted) return;

    unsigned char Data[8];
    Data[0] = (unsigned char)(TaskID & 0xFF);
    Data[1] = (unsigned char)((TaskID >> 8) & 0xFF);
    Data[2] = (unsigned char)((TaskID >> 16) & 0xFF);
    Data[3] = (unsigned char)((TaskID >> 24) & 0xFF);
    Data[4] = (unsigned char)(NewPriority & 0xFF);
    Data[5] = (unsigned char)((NewPriority >> 8) & 0xFF);
    Data[6] = (unsigned char)((NewPriority >> 16) & 0xFF);
    Data[7] = (unsigned char)((NewPriority >> 24) & 0xFF);

    _SendEvent(SYSVIEW_EVT_ID_TASK_PRIO, Data, 8);
}
