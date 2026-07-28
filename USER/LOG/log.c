#include "log.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

// Module/Level name tables
const char *LogModuleNames[LOG_MODULE_MAX] = {
    "TOUCH", "LCD", "SYSTEM"
};

const char *LogLevelNames[5] = {
    "", "ERROR", "WARNING", "INFO", "DEBUG"
};

// Global variables
LogBuffer g_logBuffer;
LogConfig g_logConfig;

// Initialize log system
void Log_Init(void)
{
    g_logBuffer.head = 0;
    g_logBuffer.tail = 0;
    g_logBuffer.count = 0;
    g_logBuffer.overflow = false;

    for (int i = 0; i < LOG_MODULE_MAX; i++) {
        g_logConfig.enabled[i] = true;
        g_logConfig.level[i] = LOG_LEVEL_INFO;
    }
    // DO NOT call Log_Write here - USART may not be ready
}

// Write log entry
void Log_Write(LogModule module, LogLevel level, const char *fmt, ...)
{
    if (!g_logConfig.enabled[module]) return;
    if (level > g_logConfig.level[module]) return;

    LogEntry entry;
    entry.timestamp = HAL_GetTick();
    entry.level = level;
    entry.module = module;

    va_list args;
    va_start(args, fmt);
    vsnprintf(entry.message, sizeof(entry.message), fmt, args);
    va_end(args);

    // Store in buffer
    if (g_logBuffer.count >= LOG_BUFFER_SIZE) {
        g_logBuffer.tail = (g_logBuffer.tail + 1) % LOG_BUFFER_SIZE;
        g_logBuffer.count--;
        g_logBuffer.overflow = true;
    }
    memcpy(&g_logBuffer.entries[g_logBuffer.head], &entry, sizeof(LogEntry));
    g_logBuffer.head = (g_logBuffer.head + 1) % LOG_BUFFER_SIZE;
    g_logBuffer.count++;

    // Output via USART
    char buf[128];
    snprintf(buf, sizeof(buf), "[%s][%09lums][%s] %s\r\n",
             LogModuleNames[module],
             (unsigned long)entry.timestamp,
             LogLevelNames[level],
             entry.message);
    USART1_SendString(buf);
}

// Set module log level
void Log_SetLevel(LogModule module, LogLevel level)
{
    if (module < LOG_MODULE_MAX) {
        g_logConfig.level[module] = level;
    }
}

// Enable/disable module
void Log_Enable(LogModule module, bool enable)
{
    if (module < LOG_MODULE_MAX) {
        g_logConfig.enabled[module] = enable;
    }
}

// Dump all logs
void Log_Dump(void)
{
    USART1_SendString("=== Log Dump ===\r\n");
    uint16_t idx = g_logBuffer.tail;
    for (int i = 0; i < g_logBuffer.count; i++) {
        LogEntry *e = &g_logBuffer.entries[idx];
        char buf[128];
        snprintf(buf, sizeof(buf), "[%s][%09lums][%s] %s\r\n",
                 LogModuleNames[e->module],
                 (unsigned long)e->timestamp,
                 LogLevelNames[e->level],
                 e->message);
        USART1_SendString(buf);
        idx = (idx + 1) % LOG_BUFFER_SIZE;
    }
    USART1_SendString("=== End ===\r\n");
}

// Clear log buffer
void Log_Clear(void)
{
    g_logBuffer.head = 0;
    g_logBuffer.tail = 0;
    g_logBuffer.count = 0;
    g_logBuffer.overflow = false;
}
