/**
 * @file SEGGER_RTT.h
 * @brief SEGGER Real-Time Transfer (RTT) header
 */

#ifndef SEGGER_RTT_H
#define SEGGER_RTT_H

#include <stdint.h>

/**
 * @brief Initialize RTT control block
 */
void SEGGER_RTT_Init(void);

/**
 * @brief Write string to RTT channel 0
 * @param s String to write
 * @return Number of bytes written
 */
unsigned SEGGER_RTT_WriteString(unsigned bufferIndex, const char *s);

/**
 * @brief Write data to RTT channel
 * @param bufferIndex Channel index
 * @param pBuffer Data to write
 * @param NumBytes Number of bytes
 * @return Number of bytes written
 */
unsigned SEGGER_RTT_Write(unsigned bufferIndex, const void *pBuffer, unsigned NumBytes);

/**
 * @brief Write character to RTT channel 0
 * @param c Character to write
 * @return Character written
 */
int SEGGER_RTT_PutChar(unsigned bufferIndex, char c);

/**
 * @brief printf-style output to RTT
 * @param bufferIndex Channel index
 * @param fmt Format string
 * @return Number of characters written
 */
int SEGGER_RTT_printf(unsigned bufferIndex, const char *fmt, ...);

#endif /* SEGGER_RTT_H */
