/**
 * @file SEGGER_RTT.c
 * @brief SEGGER RTT (Real-Time Transfer) core implementation
 *
 * This is a minimal, correct implementation of SEGGER RTT for STM32F407ZG.
 * It provides non-blocking write functions for logging from callbacks and interrupts.
 *
 * Key design decisions:
 * - Uses __disable_irq()/__enable_irq() for critical sections (safe for Cortex-M4)
 * - Non-blocking writes: drops data if buffer is full (safest for LVGL callbacks)
 * - RTT control block placed in .rtt_block section (found by debugger)
 * - Compatible with Keil ARMCC V5.06
 */

#include "SEGGER_RTT.h"
#include <string.h>

/*********************************************************************
 *
 *       Static data: Buffers for RTT channels
 *
 *       These are statically allocated to avoid heap usage.
 *       Channel 0 (LVGL): 1024 bytes
 *       Channel 1 (App):  512 bytes
 *
 **********************************************************************
 */

/**
 * @brief Up-buffer for Channel 0 (LVGL logs)
 */
static char _acUpBuffer0[SEGGER_RTT_BUFFER_SIZE_UP];

/**
 * @brief Up-buffer for Channel 1 (App logs)
 * Size is 512 bytes, suitable for application log messages.
 */
#define SEGGER_RTT_BUFFER_SIZE_UP_CH1   512u
static char _acUpBuffer1[SEGGER_RTT_BUFFER_SIZE_UP_CH1];

/**
 * @brief Up-buffer for Channel 2 (SystemView)
 * Size is 256 bytes, suitable for SystemView events.
 */
#define SEGGER_RTT_BUFFER_SIZE_UP_CH2   256u
static char _acUpBuffer2[SEGGER_RTT_BUFFER_SIZE_UP_CH2];

/**
 * @brief Down-buffer for Channel 0 (host -> target, reserved)
 */
static char _acDownBuffer0[SEGGER_RTT_BUFFER_SIZE_DOWN];

/**
 * @brief Down-buffer for Channel 1 (host -> target, reserved)
 */
static char _acDownBuffer1[16];

/*********************************************************************
 *
 *       RTT Control Block
 *
 *       Placed in .rtt_block section so the debugger can find it.
 *       The section must be defined in the linker script.
 *
 *       For Keil ARMCC, we use __attribute__((section("rtt_block")))
 *       which maps to the .rtt_block section.
 *
 **********************************************************************
 */

/**
 * @brief RTT Control Block instance
 *
 * This is the main control structure that the debugger looks for.
 * It must be at a known location (via section placement) or the
 * debugger must scan RAM for the "SEGGER RTT" identification string.
 */
#if defined(__ARMCC_VERSION)  /* ARM Compiler (Keil) */
__attribute__((section("rtt_block"), used, zero_init))
SEGGER_RTT_CB _SEGGER_RTT;
#elif defined(__GNUC__)  /* GCC */
__attribute__((section(".rtt_block"), used))
SEGGER_RTT_CB _SEGGER_RTT = {0};
#else  /* Other compilers */
SEGGER_RTT_CB _SEGGER_RTT;
#endif

/*********************************************************************
 *
 *       Static helper functions
 *
 **********************************************************************
 */

/**
 * @brief Write data to a ring buffer (internal)
 *
 * This is the core ring buffer write function.
 * It handles wrap-around and respects the buffer mode.
 *
 * @param pRing    Pointer to the up-buffer structure
 * @param pData    Pointer to data to write
 * @param NumBytes Number of bytes to write
 *
 * @return Number of bytes actually written
 */
static unsigned _WriteNoLock(SEGGER_RTT_BUFFER_UP* pRing, const char* pData, unsigned NumBytes) {
    unsigned Avail;
    unsigned WriteOff;
    unsigned Remain;
    unsigned NumBytesWritten;

    /* Calculate available space */
    WriteOff = pRing->WrOff;
    if (WriteOff >= pRing->RdOff) {
        /* Write pointer is ahead of read pointer */
        Avail = pRing->SizeOfBuffer - (WriteOff - pRing->RdOff) - 1u;
    } else {
        /* Read pointer is ahead of write pointer */
        Avail = pRing->RdOff - WriteOff - 1u;
    }

    /* Check if we have enough space */
    if (Avail == 0u) {
        /* Buffer is completely full */
        return 0u;
    }

    /* Limit to available space */
    if (NumBytes > Avail) {
        /* Buffer mode handling */
        if ((pRing->Flags & SEGGER_RTT_MODE_MASK) == SEGGER_RTT_MODE_NO_BLOCK_SKIP) {
            /* Skip mode: don't write anything if we can't fit all data */
            return 0u;
        }
        /* Trim mode: write as much as we can */
        NumBytes = Avail;
    }

    NumBytesWritten = NumBytes;

    /* Write data with wrap-around */
    Remain = pRing->SizeOfBuffer - WriteOff;
    if (Remain > NumBytes) {
        /* No wrap needed */
        memcpy(pRing->pBuffer + WriteOff, pData, NumBytes);
        WriteOff += NumBytes;
    } else {
        /* Wrap needed */
        memcpy(pRing->pBuffer + WriteOff, pData, Remain);
        NumBytes -= Remain;
        memcpy(pRing->pBuffer, pData + Remain, NumBytes);
        WriteOff = NumBytes;
    }

    /* Update write offset */
    pRing->WrOff = WriteOff;

    return NumBytesWritten;
}

/*********************************************************************
 *
 *       Global API functions
 *
 **********************************************************************
 */

/**
 * @brief Initialize RTT control block and buffers.
 *
 * This function initializes the RTT control block with:
 * - Identification string (so debugger can find it)
 * - Channel 0: LVGL logs (1024 bytes, non-blocking trim mode)
 * - Channel 1: App logs (512 bytes, non-blocking trim mode)
 *
 * Must be called once before any RTT write functions.
 *
 * @return 0 on success
 */
int SEGGER_RTT_Init(void) {
    int i;

    /* Initialize control block header */
    memset(&_SEGGER_RTT, 0, sizeof(_SEGGER_RTT));

    /* Set identification string */
    memcpy(_SEGGER_RTT.acID, SEGGER_RTT_IDENT_STRING, sizeof(_SEGGER_RTT.acID));

    /* Set buffer counts */
    _SEGGER_RTT.MaxNumUpBuffers   = SEGGER_RTT_MAX_NUM_UP_BUFFERS;
    _SEGGER_RTT.MaxNumDownBuffers = SEGGER_RTT_MAX_NUM_DOWN_BUFFERS;

    /* Initialize up-buffer 0: LVGL logs */
    _SEGGER_RTT.aUpBuffers[0].sName        = RTT_CHANNEL_NAME_LVGL;
    _SEGGER_RTT.aUpBuffers[0].pBuffer      = _acUpBuffer0;
    _SEGGER_RTT.aUpBuffers[0].SizeOfBuffer = sizeof(_acUpBuffer0);
    _SEGGER_RTT.aUpBuffers[0].WrOff        = 0u;
    _SEGGER_RTT.aUpBuffers[0].RdOff        = 0u;
    _SEGGER_RTT.aUpBuffers[0].Flags        = SEGGER_RTT_MODE_NO_BLOCK_TRIM;

    /* Initialize up-buffer 1: App logs */
    _SEGGER_RTT.aUpBuffers[1].sName        = RTT_CHANNEL_NAME_APP;
    _SEGGER_RTT.aUpBuffers[1].pBuffer      = _acUpBuffer1;
    _SEGGER_RTT.aUpBuffers[1].SizeOfBuffer = sizeof(_acUpBuffer1);
    _SEGGER_RTT.aUpBuffers[1].WrOff        = 0u;
    _SEGGER_RTT.aUpBuffers[1].RdOff        = 0u;
    _SEGGER_RTT.aUpBuffers[1].Flags        = SEGGER_RTT_MODE_NO_BLOCK_TRIM;

    /* Initialize up-buffer 2: SystemView */
    _SEGGER_RTT.aUpBuffers[2].sName        = "SystemView";
    _SEGGER_RTT.aUpBuffers[2].pBuffer      = _acUpBuffer2;
    _SEGGER_RTT.aUpBuffers[2].SizeOfBuffer = sizeof(_acUpBuffer2);
    _SEGGER_RTT.aUpBuffers[2].WrOff        = 0u;
    _SEGGER_RTT.aUpBuffers[2].RdOff        = 0u;
    _SEGGER_RTT.aUpBuffers[2].Flags        = SEGGER_RTT_MODE_NO_BLOCK_TRIM;

    /* Initialize down-buffer 0: Reserved */
    _SEGGER_RTT.aDownBuffers[0].sName        = "Terminal";
    _SEGGER_RTT.aDownBuffers[0].pBuffer      = _acDownBuffer0;
    _SEGGER_RTT.aDownBuffers[0].SizeOfBuffer = sizeof(_acDownBuffer0);
    _SEGGER_RTT.aDownBuffers[0].WrOff        = 0u;
    _SEGGER_RTT.aDownBuffers[0].RdOff        = 0u;
    _SEGGER_RTT.aDownBuffers[0].Flags        = SEGGER_RTT_MODE_NO_BLOCK_TRIM;

    /* Initialize down-buffer 1: Reserved */
    _SEGGER_RTT.aDownBuffers[1].sName        = "Terminal1";
    _SEGGER_RTT.aDownBuffers[1].pBuffer      = _acDownBuffer1;
    _SEGGER_RTT.aDownBuffers[1].SizeOfBuffer = sizeof(_acDownBuffer1);
    _SEGGER_RTT.aDownBuffers[1].WrOff        = 0u;
    _SEGGER_RTT.aDownBuffers[1].RdOff        = 0u;
    _SEGGER_RTT.aDownBuffers[1].Flags        = SEGGER_RTT_MODE_NO_BLOCK_TRIM;

    return 0;
}

/**
 * @brief Write a null-terminated string to an RTT up-buffer.
 *
 * This function is interrupt-safe. It uses SEGGER_RTT_LOCK/UNLOCK
 * to protect the buffer from concurrent access.
 *
 * @param BufferIndex  Index of the up-buffer (0 = LVGL, 1 = App)
 * @param s            Null-terminated string to write
 *
 * @return Number of bytes written (excluding null terminator)
 */
unsigned SEGGER_RTT_WriteString(unsigned BufferIndex, const char* s) {
    unsigned NumBytes;
    unsigned Status;

    /* Calculate string length */
    NumBytes = 0u;
    while (s[NumBytes] != '\0') {
        NumBytes++;
    }

    /* Write to buffer with lock */
    SEGGER_RTT_LOCK();
    Status = _WriteNoLock(&_SEGGER_RTT.aUpBuffers[BufferIndex], s, NumBytes);
    SEGGER_RTT_UNLOCK();

    return Status;
}

/**
 * @brief Write data to an RTT up-buffer.
 *
 * This function is interrupt-safe. It writes raw bytes to the buffer.
 *
 * @param BufferIndex  Index of the up-buffer (0 = LVGL, 1 = App)
 * @param pBuffer      Pointer to data to write
 * @param NumBytes     Number of bytes to write
 *
 * @return Number of bytes actually written
 */
unsigned SEGGER_RTT_Write(unsigned BufferIndex, const void* pBuffer, unsigned NumBytes) {
    unsigned Status;

    SEGGER_RTT_LOCK();
    Status = _WriteNoLock(&_SEGGER_RTT.aUpBuffers[BufferIndex], (const char*)pBuffer, NumBytes);
    SEGGER_RTT_UNLOCK();

    return Status;
}

/**
 * @brief Write a single character to an RTT up-buffer.
 *
 * @param BufferIndex  Index of the up-buffer
 * @param c            Character to write
 *
 * @return 1 if character was written, 0 if buffer was full
 */
unsigned SEGGER_RTT_PutChar(unsigned BufferIndex, char c) {
    unsigned Status;

    SEGGER_RTT_LOCK();
    Status = _WriteNoLock(&_SEGGER_RTT.aUpBuffers[BufferIndex], &c, 1u);
    SEGGER_RTT_UNLOCK();

    return (Status > 0u) ? 1u : 0u;
}

/**
 * @brief Check if there is data in a down-buffer.
 *
 * @param BufferIndex  Index of the down-buffer
 *
 * @return Number of bytes available to read
 */
unsigned SEGGER_RTT_HasData(unsigned BufferIndex) {
    SEGGER_RTT_BUFFER_DOWN* pRing;
    unsigned WrOff;
    unsigned RdOff;

    pRing = &_SEGGER_RTT.aDownBuffers[BufferIndex];
    WrOff = pRing->WrOff;
    RdOff = pRing->RdOff;

    if (WrOff >= RdOff) {
        return WrOff - RdOff;
    } else {
        return pRing->SizeOfBuffer - (RdOff - WrOff);
    }
}

/**
 * @brief Read data from a down-buffer.
 *
 * @param BufferIndex  Index of the down-buffer
 * @param pBuffer      Pointer to buffer to receive data
 * @param BufferSize   Size of the receive buffer
 *
 * @return Number of bytes actually read
 */
unsigned SEGGER_RTT_Read(unsigned BufferIndex, void* pBuffer, unsigned BufferSize) {
    SEGGER_RTT_BUFFER_DOWN* pRing;
    unsigned Avail;
    unsigned RdOff;
    unsigned Remain;
    unsigned NumBytesRead;
    char* pDest;

    pRing = &_SEGGER_RTT.aDownBuffers[BufferIndex];
    pDest = (char*)pBuffer;

    /* Calculate available data */
    RdOff = pRing->RdOff;
    if (pRing->WrOff >= RdOff) {
        Avail = pRing->WrOff - RdOff;
    } else {
        Avail = pRing->SizeOfBuffer - (RdOff - pRing->WrOff);
    }

    /* Limit to requested size */
    if (BufferSize > Avail) {
        BufferSize = Avail;
    }

    if (BufferSize == 0u) {
        return 0u;
    }

    NumBytesRead = BufferSize;

    /* Read with wrap-around */
    Remain = pRing->SizeOfBuffer - RdOff;
    if (Remain > BufferSize) {
        /* No wrap needed */
        memcpy(pDest, pRing->pBuffer + RdOff, BufferSize);
        RdOff += BufferSize;
    } else {
        /* Wrap needed */
        memcpy(pDest, pRing->pBuffer + RdOff, Remain);
        BufferSize -= Remain;
        memcpy(pDest + Remain, pRing->pBuffer, BufferSize);
        RdOff = BufferSize;
    }

    /* Update read offset */
    pRing->RdOff = RdOff;

    return NumBytesRead;
}

/**
 * @brief Get number of bytes used in an up-buffer.
 *
 * Useful for monitoring buffer usage.
 *
 * @param BufferIndex  Index of the up-buffer
 *
 * @return Number of bytes in the buffer
 */
unsigned SEGGER_RTT_GetBytesInBuffer(unsigned BufferIndex) {
    SEGGER_RTT_BUFFER_UP* pRing;
    unsigned WrOff;
    unsigned RdOff;

    pRing = &_SEGGER_RTT.aUpBuffers[BufferIndex];
    WrOff = pRing->WrOff;
    RdOff = pRing->RdOff;

    if (WrOff >= RdOff) {
        return WrOff - RdOff;
    } else {
        return pRing->SizeOfBuffer - (RdOff - WrOff);
    }
}

/**
 * @brief Configure an up-buffer with custom size and name.
 *
 * Call this after SEGGER_RTT_Init() to customize channel properties.
 * Useful if you want to use a different buffer size or location.
 *
 * @param BufferIndex    Index of the up-buffer to configure
 * @param sName          Name string (shown in debugger)
 * @param pBuffer        Pointer to user-allocated buffer (must remain valid!)
 * @param BufferSize     Size of the buffer in bytes
 * @param Flags          Mode flags (SEGGER_RTT_MODE_*)
 *
 * @return 0 on success, -1 on error (invalid index)
 */
int SEGGER_RTT_ConfigUpBuffer(unsigned BufferIndex, const char* sName, void* pBuffer, unsigned BufferSize, unsigned Flags) {
    SEGGER_RTT_BUFFER_UP* pRing;

    /* Validate index */
    if (BufferIndex >= (unsigned)_SEGGER_RTT.MaxNumUpBuffers) {
        return -1;
    }

    /* Validate parameters */
    if (pBuffer == NULL || BufferSize < 2u) {
        return -1;
    }

    SEGGER_RTT_LOCK();

    pRing = &_SEGGER_RTT.aUpBuffers[BufferIndex];
    pRing->sName        = sName;
    pRing->pBuffer      = (char*)pBuffer;
    pRing->SizeOfBuffer = BufferSize;
    pRing->WrOff        = 0u;
    pRing->RdOff        = 0u;
    pRing->Flags        = Flags & SEGGER_RTT_MODE_MASK;

    SEGGER_RTT_UNLOCK();

    return 0;
}
// Force recompile

