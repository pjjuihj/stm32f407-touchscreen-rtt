#ifndef __LOG_H
#define __LOG_H

#include "common.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>

// ==================== Log Levels ====================
typedef enum {
    LOG_LEVEL_ERROR = 1,
    LOG_LEVEL_WARNING = 2,
    LOG_LEVEL_INFO = 3,
    LOG_LEVEL_DEBUG = 4
} LogLevel;

// ==================== Log Modules ====================
typedef enum {
    LOG_MODULE_TOUCH = 0,
    LOG_MODULE_LCD,
    LOG_MODULE_SYSTEM,
    LOG_MODULE_TEST,
    LOG_MODULE_MAX
} LogModule;

// Module name strings (for display)
extern const char *LogModuleNames[LOG_MODULE_MAX];
extern const char *LogLevelNames[5]; // index 0 unused, 1-4 valid

// ==================== Log Entry ====================
typedef struct {
    uint32_t timestamp;      // Timestamp in ms (from HAL_GetTick)
    LogLevel level;          // Log level
    LogModule module;        // Module that produced the log
    char message[64];        // Log message content
} LogEntry;

// ==================== Log Buffer ====================
#define LOG_BUFFER_SIZE  64   // Default buffer size (entries)

typedef struct {
    LogEntry entries[LOG_BUFFER_SIZE];
    uint16_t head;           // Write position
    uint16_t tail;           // Read position
    uint16_t count;          // Current entry count
    bool overflow;           // Overflow flag
} LogBuffer;

// Buffer statistics
typedef struct {
    uint16_t total_writes;    // Total writes since init
    uint16_t overflow_count;  // Overflow count
    uint16_t current_usage;   // Current usage (%)
} BufferStats;

// ==================== Log Config ====================
typedef struct {
    bool enabled[LOG_MODULE_MAX];     // Per-module enable
    LogLevel level[LOG_MODULE_MAX];   // Per-module level
    bool test_mode;                   // Test mode flag
} LogConfig;

// ==================== Public API ====================

// Initialize log system with specified buffer size
void Log_Init(uint16_t buffer_size);

// Write a log entry
void Log_Write(LogModule module, LogLevel level, const char *fmt, ...);

// Set module log level
void Log_SetLevel(LogModule module, LogLevel level);

// Enable/disable module logging
void Log_Enable(LogModule module, bool enable);

// Dump all buffered logs via USART
void Log_Dump(void);

// Clear log buffer
void Log_Clear(void);

// Process a command string
void Log_ProcessCommand(const char *cmd);

// Enable/disable test mode
void Log_TestMode(bool enable);

// ==================== Buffer API ====================

// Initialize buffer
void Buffer_Init(LogBuffer *buf);

// Write entry to buffer (returns false if full/overflow)
bool Buffer_Write(LogBuffer *buf, LogEntry *entry);

// Read entry from buffer (returns false if empty)
bool Buffer_Read(LogBuffer *buf, LogEntry *entry);

// Clear buffer
void Buffer_Clear(LogBuffer *buf);

// Get current count
uint16_t Buffer_Count(LogBuffer *buf);

// Check if buffer is full
bool Buffer_IsFull(LogBuffer *buf);

// Get buffer statistics
BufferStats Buffer_GetStats(LogBuffer *buf);

#endif
