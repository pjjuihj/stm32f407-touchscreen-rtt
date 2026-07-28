#ifndef __LOG_H
#define __LOG_H

#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

// Log levels
typedef enum {
    LOG_LEVEL_ERROR = 1,
    LOG_LEVEL_WARNING = 2,
    LOG_LEVEL_INFO = 3,
    LOG_LEVEL_DEBUG = 4
} LogLevel;

// Log modules
typedef enum {
    LOG_MODULE_TOUCH = 0,
    LOG_MODULE_LCD,
    LOG_MODULE_SYSTEM,
    LOG_MODULE_MAX
} LogModule;

// Module name strings
extern const char *LogModuleNames[LOG_MODULE_MAX];
extern const char *LogLevelNames[5];

// Log entry
typedef struct {
    uint32_t timestamp;
    LogLevel level;
    LogModule module;
    char message[64];
} LogEntry;

// Log buffer size (increased to 64)
#define LOG_BUFFER_SIZE 64

// Log buffer
typedef struct {
    LogEntry entries[LOG_BUFFER_SIZE];
    uint16_t head;
    uint16_t tail;
    uint16_t count;
    bool overflow;
} LogBuffer;

// Log config
typedef struct {
    bool enabled[LOG_MODULE_MAX];
    LogLevel level[LOG_MODULE_MAX];
} LogConfig;

// Global variables
extern LogBuffer g_logBuffer;
extern LogConfig g_logConfig;

// Initialize log system (DO NOT call Log_Write here)
void Log_Init(void);

// Write log entry
void Log_Write(LogModule module, LogLevel level, const char *fmt, ...);

// Set module log level
void Log_SetLevel(LogModule module, LogLevel level);

// Enable/disable module
void Log_Enable(LogModule module, bool enable);

// Dump all logs
void Log_Dump(void);

// Clear log buffer
void Log_Clear(void);

#endif
