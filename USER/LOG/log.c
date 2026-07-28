#include "log.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// ==================== Module/Level Name Tables ====================
const char *LogModuleNames[LOG_MODULE_MAX] = {
    "TOUCH", "LCD", "SYSTEM", "TEST"
};

const char *LogLevelNames[5] = {
    "", "ERROR", "WARNING", "INFO", "DEBUG"
};

// ==================== Global Variables (exposed for testing) ====================
LogBuffer g_logBuffer;
LogConfig g_logConfig;
static uint16_t g_writeCount = 0;

// ==================== Buffer Implementation ====================

void Buffer_Init(LogBuffer *buf)
{
    buf->head = 0;
    buf->tail = 0;
    buf->count = 0;
    buf->overflow = false;
    memset(buf->entries, 0, sizeof(buf->entries));
}

bool Buffer_Write(LogBuffer *buf, LogEntry *entry)
{
    if (buf->count >= LOG_BUFFER_SIZE) {
        // Buffer full - overwrite oldest (advance tail)
        buf->tail = (buf->tail + 1) % LOG_BUFFER_SIZE;
        buf->count--;
        buf->overflow = true;
    }

    memcpy(&buf->entries[buf->head], entry, sizeof(LogEntry));
    buf->head = (buf->head + 1) % LOG_BUFFER_SIZE;
    buf->count++;
    g_writeCount++;
    return true;
}

bool Buffer_Read(LogBuffer *buf, LogEntry *entry)
{
    if (buf->count == 0) {
        return false;
    }

    memcpy(entry, &buf->entries[buf->tail], sizeof(LogEntry));
    buf->tail = (buf->tail + 1) % LOG_BUFFER_SIZE;
    buf->count--;
    return true;
}

void Buffer_Clear(LogBuffer *buf)
{
    buf->head = 0;
    buf->tail = 0;
    buf->count = 0;
    buf->overflow = false;
}

uint16_t Buffer_Count(LogBuffer *buf)
{
    return buf->count;
}

bool Buffer_IsFull(LogBuffer *buf)
{
    return buf->count >= LOG_BUFFER_SIZE;
}

BufferStats Buffer_GetStats(LogBuffer *buf)
{
    BufferStats stats;
    stats.total_writes = g_writeCount;
    stats.overflow_count = buf->overflow ? 1 : 0;
    stats.current_usage = (buf->count * 100) / LOG_BUFFER_SIZE;
    return stats;
}

// ==================== Log System API ====================

void Log_Init(void)
{
    Buffer_Init(&g_logBuffer);

    // Default: all modules enabled at INFO level
    for (int i = 0; i < LOG_MODULE_MAX; i++) {
        g_logConfig.enabled[i] = true;
        g_logConfig.level[i] = LOG_LEVEL_INFO;
    }
    g_logConfig.test_mode = false;
    g_writeCount = 0;

    Log_Write(LOG_MODULE_SYSTEM, LOG_LEVEL_INFO, "Log system initialized");
}

void Log_Write(LogModule module, LogLevel level, const char *fmt, ...)
{
    // Check if module is enabled and level is sufficient
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
    Buffer_Write(&g_logBuffer, &entry);

    // Output via USART immediately
    USART1_Printf("[%s][%09lums][%s] %s\r\n",
                  LogModuleNames[module],
                  (unsigned long)entry.timestamp,
                  LogLevelNames[level],
                  entry.message);
}

void Log_SetLevel(LogModule module, LogLevel level)
{
    if (module < LOG_MODULE_MAX) {
        g_logConfig.level[module] = level;
        Log_Write(LOG_MODULE_SYSTEM, LOG_LEVEL_INFO,
                  "Module %s level set to %s",
                  LogModuleNames[module], LogLevelNames[level]);
    }
}

void Log_Enable(LogModule module, bool enable)
{
    if (module < LOG_MODULE_MAX) {
        g_logConfig.enabled[module] = enable;
        Log_Write(LOG_MODULE_SYSTEM, LOG_LEVEL_INFO,
                  "Module %s %s",
                  LogModuleNames[module], enable ? "enabled" : "disabled");
    }
}

void Log_Dump(void)
{
    LogEntry entry;
    uint16_t count = Buffer_Count(&g_logBuffer);

    USART1_Printf("=== Log Dump (%d entries) ===\r\n", count);

    // Read and display all entries
    static LogBuffer tempBuf;
    Buffer_Init(&tempBuf);

    while (Buffer_Read(&g_logBuffer, &entry)) {
        USART1_Printf("[%s][%09lums][%s] %s\r\n",
                      LogModuleNames[entry.module],
                      (unsigned long)entry.timestamp,
                      LogLevelNames[entry.level],
                      entry.message);
        Buffer_Write(&tempBuf, &entry);
    }

    // Restore buffer
    memcpy(&g_logBuffer, &tempBuf, sizeof(LogBuffer));
    USART1_Printf("=== End of Dump ===\r\n");
}

void Log_Clear(void)
{
    Buffer_Clear(&g_logBuffer);
    USART1_Printf("Log buffer cleared\r\n");
}

void Log_TestMode(bool enable)
{
    g_logConfig.test_mode = enable;
    Log_Write(LOG_MODULE_SYSTEM, LOG_LEVEL_INFO,
              "Test mode %s", enable ? "enabled" : "disabled");
}

// ==================== Command Parser ====================

// Helper: compare case-insensitive
static bool str_match(const char *a, const char *b)
{
    while (*a && *b) {
        if ((*a | 0x20) != (*b | 0x20)) return false;
        a++;
        b++;
    }
    return (*a == *b);
}

// Helper: parse module name
static LogModule parse_module(const char *name)
{
    for (int i = 0; i < LOG_MODULE_MAX; i++) {
        if (str_match(name, LogModuleNames[i])) {
            return (LogModule)i;
        }
    }
    return LOG_MODULE_MAX; // invalid
}

void Log_ProcessCommand(const char *cmd)
{
    char buf[64];
    char *token;
    char *saveptr;

    strncpy(buf, cmd, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    token = strtok_r(buf, " ", &saveptr);
    if (!token) return;

    // STATUS command
    if (str_match(token, "STATUS")) {
        USART1_Printf("=== Log System Status ===\r\n");
        for (int i = 0; i < LOG_MODULE_MAX; i++) {
            USART1_Printf("  %s: %s, Level=%s\r\n",
                          LogModuleNames[i],
                          g_logConfig.enabled[i] ? "ON" : "OFF",
                          LogLevelNames[g_logConfig.level[i]]);
        }
        BufferStats stats = Buffer_GetStats(&g_logBuffer);
        USART1_Printf("  Buffer: %d/%d (%d%%)\r\n",
                      g_logBuffer.count, LOG_BUFFER_SIZE, stats.current_usage);
        return;
    }

    // DUMP command
    if (str_match(token, "DUMP")) {
        Log_Dump();
        return;
    }

    // CLEAR command
    if (str_match(token, "CLEAR")) {
        Log_Clear();
        return;
    }

    // LEVEL <1-4> command (global)
    if (str_match(token, "LEVEL")) {
        token = strtok_r(NULL, " ", &saveptr);
        if (token) {
            int lvl = atoi(token);
            if (lvl >= 1 && lvl <= 4) {
                for (int i = 0; i < LOG_MODULE_MAX; i++) {
                    g_logConfig.level[i] = (LogLevel)lvl;
                }
                USART1_Printf("All modules level set to %s\r\n",
                              LogLevelNames[lvl]);
            } else {
                USART1_Printf("Invalid level (1-4)\r\n");
            }
        }
        return;
    }

    // TEST command
    if (str_match(token, "TEST")) {
        token = strtok_r(NULL, " ", &saveptr);
        if (token) {
            extern void Test_RunAll(void);
            extern void Test_Write(void);
            extern void Test_Overflow(void);
            extern void Test_Filter(void);

            if (str_match(token, "RUN")) {
                Test_RunAll();
            } else if (str_match(token, "WRITE")) {
                Test_Write();
            } else if (str_match(token, "OVERFLOW")) {
                Test_Overflow();
            } else if (str_match(token, "FILTER")) {
                Test_Filter();
            } else if (str_match(token, "CLEAR")) {
                Log_Clear();
            } else {
                USART1_Printf("Unknown TEST command: %s\r\n", token);
            }
        }
        return;
    }

    // Module commands: <MODULE> ON/OFF/LEVEL <n>
    LogModule mod = parse_module(token);
    if (mod < LOG_MODULE_MAX) {
        token = strtok_r(NULL, " ", &saveptr);
        if (!token) return;

        if (str_match(token, "ON")) {
            Log_Enable(mod, true);
        } else if (str_match(token, "OFF")) {
            Log_Enable(mod, false);
        } else if (str_match(token, "LEVEL")) {
            token = strtok_r(NULL, " ", &saveptr);
            if (token) {
                int lvl = atoi(token);
                if (lvl >= 1 && lvl <= 4) {
                    Log_SetLevel(mod, (LogLevel)lvl);
                } else {
                    USART1_Printf("Invalid level (1-4)\r\n");
                }
            }
        } else {
            USART1_Printf("Unknown action: %s\r\n", token);
        }
    } else {
        USART1_Printf("Unknown command: %s\r\n", cmd);
    }
}
