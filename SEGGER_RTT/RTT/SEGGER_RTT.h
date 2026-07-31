/**
 * @file SEGGER_RTT.h
 * @brief SEGGER RTT (Real-Time Transfer) core header
 *
 * SEGGER's RTT is a technology for interactive user I/O with embedded targets.
 * It combines the advantages of SWO (fast, no pins needed) and semihosting
 * (bidirectional communication, no extra pins) without their drawbacks.
 *
 * RTT uses an in-memory ring buffer for communication, allowing:
 * - Zero-pin logging (uses existing SWD/JTAG connection)
 * - Very high speed (multiple MB/s)
 * - Non-blocking writes (safe in callbacks and interrupts)
 * - Bidirectional communication (host -> target and target -> host)
 */

#ifndef RTT_CORE_H
#define RTT_CORE_H

#include <stdlib.h>
#include "SEGGER_RTT_Conf.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
 *
 *       Defines, fixed
 *
 **********************************************************************
 */

/**
 * Identification string for RTT control block.
 * Used by debugger to find the control block in RAM.
 */
#define SEGGER_RTT_IDENT_STRING            "SEGGER RTT\0\0\0\0\0\0"

/**
 * RTT buffer flags
 */
#define SEGGER_RTT_MODE_MASK                0x03u
#define SEGGER_RTT_MODE_NO_BLOCK_SKIP       0x00u   /* Skip (don't write) if buffer full */
#define SEGGER_RTT_MODE_NO_BLOCK_TRIM       0x01u   /* Trim data to fit if buffer full */
#define SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL  0x02u   /* Block until buffer has space (NOT recommended for callbacks!) */

/**
 * Return values for SEGGER_RTT_Write* functions
 */
#define SEGGER_RTT_HASDATA(Index)           (SEGGER_RTT.pUpBuffers[(Index)].WrOff - SEGGER_RTT.pUpBuffers[(Index)].RdOff)

/**
 * Channel names for pyOCD/Ozone display
 */
#define RTT_CHANNEL_NAME_LVGL               "LVGL_LOG"
#define RTT_CHANNEL_NAME_APP                "APP_LOG"

/*********************************************************************
 *
 *       Types
 *
 **********************************************************************
 */

/**
 * @brief RTT up-buffer (target -> host)
 *
 * This structure describes a buffer that is used to send data
 * from the target to the host (debugger).
 */
typedef struct {
    const char*    sName;          /* Optional: Name of the channel (for display in debugger) */
    char*          pBuffer;        /* Pointer to the data buffer */
    unsigned       SizeOfBuffer;   /* Size of the buffer in bytes */
    volatile unsigned WrOff;       /* Write offset (next write position) */
    volatile unsigned RdOff;       /* Read offset (next read position by host) */
    unsigned       Flags;          /* Mode flags (blocking behavior) */
} SEGGER_RTT_BUFFER_UP;

/**
 * @brief RTT down-buffer (host -> target)
 *
 * This structure describes a buffer that is used to receive data
 * from the host to the target.
 */
typedef struct {
    const char*    sName;          /* Optional: Name of the channel (for display in debugger) */
    char*          pBuffer;        /* Pointer to the data buffer */
    unsigned       SizeOfBuffer;   /* Size of the buffer in bytes */
    volatile unsigned WrOff;       /* Write offset (next write position by host) */
    volatile unsigned RdOff;       /* Read offset (next read position) */
    unsigned       Flags;          /* Mode flags (blocking behavior) */
} SEGGER_RTT_BUFFER_DOWN;

/**
 * @brief RTT control block
 *
 * This is the main control structure for RTT.
 * It is placed at a fixed location in RAM (via .rtt_block section)
 * so that the debugger can find it.
 *
 * MUST be placed in a section named "rtt_block" or at a known address.
 */
typedef struct {
    char                    acID[16];                           /* Magic ID: "SEGGER RTT\0\0\0\0\0\0" */
    int                     MaxNumUpBuffers;                    /* Maximum number of up-buffers configured */
    int                     MaxNumDownBuffers;                  /* Maximum number of down-buffers configured */
    SEGGER_RTT_BUFFER_UP    aUpBuffers[SEGGER_RTT_MAX_NUM_UP_BUFFERS];    /* Up-buffers (target -> host) */
    SEGGER_RTT_BUFFER_DOWN  aDownBuffers[SEGGER_RTT_MAX_NUM_DOWN_BUFFERS]; /* Down-buffers (host -> target) */
} SEGGER_RTT_CB;

/*********************************************************************
 *
 *       Global data
 *
 **********************************************************************
 */

/**
 * @brief RTT control block instance
 *
 * Placed in .rtt_block section so debugger can find it.
 * The section is defined in the linker script.
 */
#if defined(__ARMCC_VERSION)  /* ARM Compiler (Keil) */
#pragma push
#pragma O0
extern SEGGER_RTT_CB _SEGGER_RTT;
#define SEGGER_RTT _SEGGER_RTT
#elif defined(__IAR_SYSTEMS_ICC__)  /* IAR Compiler */
extern SEGGER_RTT_CB _SEGGER_RTT;
#define SEGGER_RTT _SEGGER_RTT
#else  /* GCC and others */
extern SEGGER_RTT_CB _SEGGER_RTT;
#define SEGGER_RTT _SEGGER_RTT
#endif

/*********************************************************************
 *
 *       API functions
 *
 **********************************************************************
 */

/**
 * @brief Initialize RTT control block and buffers.
 *
 * Must be called once before any RTT write functions.
 * Typically called early in main().
 *
 * @return 0 on success
 */
int  SEGGER_RTT_Init(void);

/**
 * @brief Write a null-terminated string to an RTT up-buffer.
 *
 * @param BufferIndex  Index of the up-buffer (0 = LVGL, 1 = App)
 * @param s            Null-terminated string to write
 *
 * @return Number of bytes written (excluding null terminator)
 */
unsigned SEGGER_RTT_WriteString(unsigned BufferIndex, const char* s);

/**
 * @brief Write data to an RTT up-buffer.
 *
 * @param BufferIndex  Index of the up-buffer (0 = LVGL, 1 = App)
 * @param pBuffer      Pointer to data to write
 * @param NumBytes     Number of bytes to write
 *
 * @return Number of bytes actually written (may be less if buffer is full)
 */
unsigned SEGGER_RTT_Write(unsigned BufferIndex, const void* pBuffer, unsigned NumBytes);

/**
 * @brief Write a single character to an RTT up-buffer.
 *
 * @param BufferIndex  Index of the up-buffer
 * @param c            Character to write
 *
 * @return 1 if character was written, 0 if buffer was full
 */
unsigned SEGGER_RTT_PutChar(unsigned BufferIndex, char c);

/**
 * @brief Check if there is data in a down-buffer.
 *
 * @param BufferIndex  Index of the down-buffer
 *
 * @return Number of bytes available to read
 */
unsigned SEGGER_RTT_HasData(unsigned BufferIndex);

/**
 * @brief Read data from a down-buffer.
 *
 * @param BufferIndex  Index of the down-buffer
 * @param pBuffer      Pointer to buffer to receive data
 * @param BufferSize   Size of the receive buffer
 *
 * @return Number of bytes actually read
 */
unsigned SEGGER_RTT_Read(unsigned BufferIndex, void* pBuffer, unsigned BufferSize);

/**
 * @brief Get number of bytes used in an up-buffer.
 *
 * @param BufferIndex  Index of the up-buffer
 *
 * @return Number of bytes in the buffer
 */
unsigned SEGGER_RTT_GetBytesInBuffer(unsigned BufferIndex);

/**
 * @brief Configure an up-buffer with custom size and name.
 *
 * Call this after SEGGER_RTT_Init() to customize channel properties.
 *
 * @param BufferIndex    Index of the up-buffer to configure
 * @param sName          Name string (shown in debugger)
 * @param pBuffer        Pointer to user-allocated buffer
 * @param BufferSize     Size of the buffer in bytes
 * @param Flags          Mode flags (SEGGER_RTT_MODE_*)
 *
 * @return 0 on success, -1 on error
 */
int  SEGGER_RTT_ConfigUpBuffer(unsigned BufferIndex, const char* sName, void* pBuffer, unsigned BufferSize, unsigned Flags);

#ifdef __ARMCC_VERSION
#pragma pop
#endif

#ifdef __cplusplus
}
#endif

#endif /* RTT_CORE_H */
