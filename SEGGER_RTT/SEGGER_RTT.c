/**
 * @file SEGGER_RTT.c
 * @brief SEGGER Real-Time Transfer (RTT) implementation
 *
 * Minimal RTT implementation for STM32F407
 * Uses a ring buffer in RAM that can be read by debug probe
 */

#include "SEGGER_RTT.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

/* RTT Control Block structure */
typedef struct {
    char acID[16];          // "SEGGER RTT"
    unsigned NumUpBuffers;  // Number of up buffers
    unsigned NumDownBuffers;// Number of down buffers
    void *pBuffer;          // Buffer pointer
    unsigned Size;          // Buffer size
    unsigned R;             // Read position
    unsigned W;             // Write position
    unsigned Flags;         // Buffer flags
} SEGGER_RTT_BUFFER;

typedef struct {
    char acID[16];
    unsigned NumUpBuffers;
    unsigned NumDownBuffers;
    SEGGER_RTT_BUFFER aUp[1];
    SEGGER_RTT_BUFFER aDown[1];
} SEGGER_RTT_CONTROL;

/* Control block placed in RAM */
__attribute__((section(".rtt_block")))
static SEGGER_RTT_CONTROL _ SEGGER_RTT_CB = {
    .acID = "SEGGER RTT",
    .NumUpBuffers = 1,
    .NumDownBuffers = 1,
    .aUp[0] = {
        .acID = "terminal",
        .pBuffer = NULL,
        .Size = 0,
        .R = 0,
        .W = 0,
        .Flags = 0
    },
    .aDown[0] = {
        .acID = "terminal",
        .pBuffer = NULL,
        .Size = 0,
        .R = 0,
        .W = 0,
        .Flags = 0
    }
};

/* Default buffer size */
#define RTT_BUF_SIZE 1024

/* Static buffers */
static char _aUpBuffer[RTT_BUF_SIZE];
static char _aDownBuffer[RTT_BUF_SIZE];

/**
 * @brief Initialize RTT
 */
void SEGGER_RTT_Init(void) {
    _SEGGER_RTT_CB.aUp[0].pBuffer = _aUpBuffer;
    _SEGGER_RTT_CB.aUp[0].Size = RTT_BUF_SIZE;
    _SEGGER_RTT_CB.aUp[0].R = 0;
    _SEGGER_RTT_CB.aUp[0].W = 0;

    _SEGGER_RTT_CB.aDown[0].pBuffer = _aDownBuffer;
    _SEGGER_RTT_CB.aDown[0].Size = RTT_BUF_SIZE;
    _SEGGER_RTT_CB.aDown[0].R = 0;
    _SEGGER_RTT_CB.aDown[0].W = 0;
}

/**
 * @brief Write data to RTT buffer
 */
unsigned SEGGER_RTT_Write(unsigned bufferIndex, const void *pBuffer, unsigned NumBytes) {
    SEGGER_RTT_BUFFER *p = &_SEGGER_RTT_CB.aUp[bufferIndex];
    const char *src = (const char *)pBuffer;
    unsigned i;

    if (p->pBuffer == NULL || p->Size == 0) {
        return 0;
    }

    for (i = 0; i < NumBytes; i++) {
        unsigned next = (p->W + 1) % p->Size;
        if (next == p->R) {
            // Buffer full, stop writing
            break;
        }
        p->pBuffer[p->W] = src[i];
        p->W = next;
    }

    return i;
}

/**
 * @brief Write string to RTT
 */
unsigned SEGGER_RTT_WriteString(unsigned bufferIndex, const char *s) {
    return SEGGER_RTT_Write(bufferIndex, s, strlen(s));
}

/**
 * @brief Write single character
 */
int SEGGER_RTT_PutChar(unsigned bufferIndex, char c) {
    SEGGER_RTT_Write(bufferIndex, &c, 1);
    return c;
}

/**
 * @brief printf-style output
 */
int SEGGER_RTT_printf(unsigned bufferIndex, const char *fmt, ...) {
    char buf[256];
    va_list args;
    int len;

    va_start(args, fmt);
    len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len > 0) {
        SEGGER_RTT_Write(bufferIndex, buf, len);
    }

    return len;
}
