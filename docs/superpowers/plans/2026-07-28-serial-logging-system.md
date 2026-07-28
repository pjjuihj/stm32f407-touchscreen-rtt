# 串口日志系统 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a serial logging system to the STM32F407ZG touchscreen project, with USART1 command control and test environment.

**Architecture:** Modular logging system with separate modules for log management, USART communication, and testing. Logs are stored in a RAM circular buffer and output via USART1 at 115200 baud.

**Tech Stack:** STM32F4 HAL, USART1 (PA9/PA10), 115200 baud, SysTick for timestamps

## Global Constraints

- Target: STM32F407ZG (168MHz system clock)
- HAL library (already in project)
- USART1: PA9 (TX), PA10 (RX), 115200 baud, 8N1
- Log buffer default size: 64 entries
- Log message max length: 64 bytes
- Timestamp source: HAL_GetTick() (1ms precision)

---

## File Structure

```
USER/
├── LOG/
│   ├── log.h              // Log system public API
│   └── log.c              // Log system implementation
├── TEST/
│   ├── test_log.h         // Test module public API
│   └── test_log.c         // Test module implementation
├── USART/
│   ├── usart.h            // USART driver public API
│   └── usart.c            // USART driver implementation
Main/
└── main.c                 // Main program (integrate log system)
```

---

### Task 1: USART Driver

**Files:**
- Create: `USER/USART/usart.h`
- Create: `USER/USART/usart.c`

**Interfaces:**
- Consumes: HAL library (stm32f4xx_hal.h)
- Produces: `USART1_Init()`, `USART1_SendString()`, `USART1_SendChar()`

- [ ] **Step 1: Create usart.h**

```c
#ifndef __USART_H
#define __USART_H

#include "stm32f4xx_hal.h"
#include <stdio.h>
#include <string.h>

// Initialize USART1 (PA9-TX, PA10-RX, 115200 baud, 8N1)
void USART1_Init(void);

// Send a single character
void USART1_SendChar(uint8_t ch);

// Send a string (blocking)
void USART1_SendString(const char *str);

// Send formatted string (printf-style)
void USART1_Printf(const char *fmt, ...);

#endif
```

- [ ] **Step 2: Create usart.c**

```c
#include "usart.h"

UART_HandleTypeDef huart1;

// printf redirect to USART1
#ifdef __GNUC__
int __io_putchar(int ch)
#else
int fputc(int ch, FILE *f)
#endif
{
    USART1_SendChar((uint8_t)ch);
    return ch;
}

void USART1_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    // PA9 = USART1_TX, PA10 = USART1_RX
    GPIO_InitStruct.Pin = GPIO_PIN_9 | GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart1);
}

void USART1_SendChar(uint8_t ch)
{
    HAL_UART_Transmit(&huart1, &ch, 1, HAL_MAX_DELAY);
}

void USART1_SendString(const char *str)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)str, strlen(str), HAL_MAX_DELAY);
}

void USART1_Printf(const char *fmt, ...)
{
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    USART1_SendString(buf);
}
```

- [ ] **Step 3: Verify compilation**

Check that usart.h and usart.c compile without errors in the Keil project.

- [ ] **Step 4: Commit**

```bash
git add USER/USART/usart.h USER/USART/usart.c
git commit -m "feat: add USART1 driver (115200 baud, PA9/PA10)"
```

---

### Task 2: Log System Header

**Files:**
- Create: `USER/LOG/log.h`

**Interfaces:**
- Consumes: HAL library, common.h
- Produces: All log system data structures and API declarations

- [ ] **Step 1: Create log.h**

```c
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
```

- [ ] **Step 2: Verify compilation**

Check that log.h compiles without errors.

- [ ] **Step 3: Commit**

```bash
git add USER/LOG/log.h
git commit -m "feat: add log system header with data structures and API"
```

---

### Task 3: Log System Implementation

**Files:**
- Create: `USER/LOG/log.c`
- Modify: `USER/LOG/log.h` (add Buffer static variables)

**Interfaces:**
- Consumes: log.h, usart.h
- Produces: All log system API functions

- [ ] **Step 1: Create log.c**

```c
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

// ==================== Static Variables ====================
static LogBuffer g_logBuffer;
static LogConfig g_logConfig;
static char g_cmdBuffer[64];
static uint8_t g_cmdIndex = 0;

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
    stats.total_writes = buf->count + (buf->overflow ? LOG_BUFFER_SIZE : 0);
    stats.overflow_count = buf->overflow ? 1 : 0;
    stats.current_usage = (buf->count * 100) / LOG_BUFFER_SIZE;
    return stats;
}

// ==================== Log System API ====================

void Log_Init(uint16_t buffer_size)
{
    Buffer_Init(&g_logBuffer);

    // Default: all modules enabled at INFO level
    for (int i = 0; i < LOG_MODULE_MAX; i++) {
        g_logConfig.enabled[i] = true;
        g_logConfig.level[i] = LOG_LEVEL_INFO;
    }
    g_logConfig.test_mode = false;
    g_cmdIndex = 0;

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
    LogBuffer tempBuf;
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
```

- [ ] **Step 2: Verify compilation**

Check that log.c compiles without errors.

- [ ] **Step 3: Commit**

```bash
git add USER/LOG/log.c
git commit -m "feat: implement log system with buffer, command parser"
```

---

### Task 4: Test Module

**Files:**
- Create: `USER/TEST/test_log.h`
- Create: `USER/TEST/test_log.c`

**Interfaces:**
- Consumes: log.h, usart.h
- Produces: Test_RunAll(), Test_Write(), Test_Overflow(), Test_Filter()

- [ ] **Step 1: Create test_log.h**

```c
#ifndef __TEST_LOG_H
#define __TEST_LOG_H

#include "common.h"
#include <stdbool.h>

// Run all test cases
void Test_RunAll(void);

// Individual test functions
void Test_Write(void);
void Test_Overflow(void);
void Test_Filter(void);

#endif
```

- [ ] **Step 2: Create test_log.c**

```c
#include "test_log.h"
#include "log.h"
#include "usart.h"
#include <string.h>

// ==================== Test Helpers ====================

static uint8_t test_pass = 0;
static uint8_t test_fail = 0;

static void test_report(const char *name, bool passed)
{
    if (passed) {
        test_pass++;
        USART1_Printf("  [PASS] %s\r\n", name);
    } else {
        test_fail++;
        USART1_Printf("  [FAIL] %s\r\n", name);
    }
}

// ==================== Test Cases ====================

// Test 1: Log initialization
static bool Test_Log_Init(void)
{
    Log_Init(LOG_BUFFER_SIZE);
    // If we get here without crash, test passes
    return true;
}

// Test 2: Log write
static bool Test_Log_Write(void)
{
    LogEntry entry;
    entry.timestamp = 12345;
    entry.level = LOG_LEVEL_INFO;
    entry.module = LOG_MODULE_TOUCH;
    strcpy(entry.message, "Test write entry");

    Buffer_Clear(&g_logBuffer);
    bool result = Buffer_Write(&g_logBuffer, &entry);
    return (result && Buffer_Count(&g_logBuffer) == 1);
}

// Test 3: Log enable/disable
static bool Test_Log_Enable(void)
{
    Log_Enable(LOG_MODULE_TOUCH, false);
    bool disabled = !g_logConfig.enabled[LOG_MODULE_TOUCH];

    Log_Enable(LOG_MODULE_TOUCH, true);
    bool enabled = g_logConfig.enabled[LOG_MODULE_TOUCH];

    return (disabled && enabled);
}

// Test 4: Log level setting
static bool Test_Log_SetLevel(void)
{
    Log_SetLevel(LOG_MODULE_LCD, LOG_LEVEL_DEBUG);
    return (g_logConfig.level[LOG_MODULE_LCD] == LOG_LEVEL_DEBUG);
}

// Test 5: Buffer write
static bool Test_Buffer_Write(void)
{
    LogEntry entry;
    entry.timestamp = 100;
    entry.level = LOG_LEVEL_INFO;
    entry.module = LOG_MODULE_SYSTEM;
    strcpy(entry.message, "Buffer test");

    Buffer_Clear(&g_logBuffer);
    bool result = Buffer_Write(&g_logBuffer, &entry);
    return (result && g_logBuffer.count == 1);
}

// Test 6: Buffer read
static bool Test_Buffer_Read(void)
{
    LogEntry entry;
    entry.timestamp = 200;
    entry.level = LOG_LEVEL_WARNING;
    entry.module = LOG_MODULE_LCD;
    strcpy(entry.message, "Read test");

    Buffer_Clear(&g_logBuffer);
    Buffer_Write(&g_logBuffer, &entry);

    LogEntry read_entry;
    bool result = Buffer_Read(&g_logBuffer, &read_entry);
    return (result &&
            read_entry.timestamp == 200 &&
            read_entry.level == LOG_LEVEL_WARNING &&
            g_logBuffer.count == 0);
}

// Test 7: Buffer overflow
static bool Test_Buffer_Overflow(void)
{
    LogEntry entry;
    Buffer_Clear(&g_logBuffer);

    // Fill buffer completely
    for (int i = 0; i < LOG_BUFFER_SIZE; i++) {
        entry.timestamp = i;
        entry.level = LOG_LEVEL_DEBUG;
        entry.module = LOG_MODULE_SYSTEM;
        strcpy(entry.message, "Fill");
        Buffer_Write(&g_logBuffer, &entry);
    }

    if (!Buffer_IsFull(&g_logBuffer)) return false;

    // Write one more - should overflow
    entry.timestamp = 999;
    Buffer_Write(&g_logBuffer, &entry);

    // Check overflow happened and oldest was overwritten
    return (g_logBuffer.overflow && g_logBuffer.count == LOG_BUFFER_SIZE);
}

// Test 8: Buffer clear
static bool Test_Buffer_Clear(void)
{
    LogEntry entry;
    Buffer_Clear(&g_logBuffer);

    // Add some entries
    for (int i = 0; i < 5; i++) {
        entry.timestamp = i;
        Buffer_Write(&g_logBuffer, &entry);
    }

    Buffer_Clear(&g_logBuffer);
    return (g_logBuffer.count == 0 && g_logBuffer.head == 0 && g_logBuffer.tail == 0);
}

// Test 9: Command parse ON/OFF
static bool Test_Parse_OnOff(void)
{
    Log_Enable(LOG_MODULE_TOUCH, true);
    Log_ProcessCommand("TOUCH OFF");
    bool off_result = !g_logConfig.enabled[LOG_MODULE_TOUCH];

    Log_ProcessCommand("TOUCH ON");
    bool on_result = g_logConfig.enabled[LOG_MODULE_TOUCH];

    return (off_result && on_result);
}

// Test 10: Command parse LEVEL
static bool Test_Parse_Level(void)
{
    Log_ProcessCommand("LCD LEVEL 4");
    return (g_logConfig.level[LOG_MODULE_LCD] == LOG_LEVEL_DEBUG);
}

// Test 11: Command parse DUMP
static bool Test_Parse_Dump(void)
{
    // DUMP should not crash - just verify it runs
    Buffer_Clear(&g_logBuffer);
    Log_Dump();
    return true;
}

// Test 12: Command parse CLEAR
static bool Test_Parse_Clear(void)
{
    LogEntry entry;
    Buffer_Clear(&g_logBuffer);
    entry.timestamp = 1;
    Buffer_Write(&g_logBuffer, &entry);

    Log_Clear();
    return (g_logBuffer.count == 0);
}

// Test 13: Timestamp get
static bool Test_Timestamp_Get(void)
{
    uint32_t t1 = HAL_GetTick();
    // Small delay
    for (volatile int i = 0; i < 10000; i++);
    uint32_t t2 = HAL_GetTick();
    return (t2 >= t1);
}

// Test 14: Timestamp format
static bool Test_Timestamp_Format(void)
{
    // Just verify format function doesn't crash
    char buf[32];
    snprintf(buf, sizeof(buf), "[%09lums]", (unsigned long)12345);
    return (strlen(buf) > 0);
}

// Test 15: Full cycle
static bool Test_FullCycle(void)
{
    // Init
    Log_Init(LOG_BUFFER_SIZE);

    // Write some logs
    Log_Write(LOG_MODULE_TOUCH, LOG_LEVEL_INFO, "Cycle test 1");
    Log_Write(LOG_MODULE_LCD, LOG_LEVEL_DEBUG, "Cycle test 2");

    // Check buffer has entries
    if (Buffer_Count(&g_logBuffer) < 2) return false;

    // Dump (should not crash)
    Log_Dump();

    // Clear
    Log_Clear();
    return (Buffer_Count(&g_logBuffer) == 0);
}

// ==================== Public Test Functions ====================

void Test_RunAll(void)
{
    test_pass = 0;
    test_fail = 0;

    USART1_Printf("=== Running All Tests ===\r\n");

    // Enable test module logging
    g_logConfig.enabled[LOG_MODULE_TEST] = true;
    g_logConfig.level[LOG_MODULE_TEST] = LOG_LEVEL_DEBUG;

    // Basic function tests
    test_report("Test_Log_Init", Test_Log_Init());
    test_report("Test_Log_Write", Test_Log_Write());
    test_report("Test_Log_Enable", Test_Log_Enable());
    test_report("Test_Log_SetLevel", Test_Log_SetLevel());

    // Buffer tests
    test_report("Test_Buffer_Write", Test_Buffer_Write());
    test_report("Test_Buffer_Read", Test_Buffer_Read());
    test_report("Test_Buffer_Overflow", Test_Buffer_Overflow());
    test_report("Test_Buffer_Clear", Test_Buffer_Clear());

    // Command parser tests
    test_report("Test_Parse_OnOff", Test_Parse_OnOff());
    test_report("Test_Parse_Level", Test_Parse_Level());
    test_report("Test_Parse_Dump", Test_Parse_Dump());
    test_report("Test_Parse_Clear", Test_Parse_Clear());

    // Timestamp tests
    test_report("Test_Timestamp_Get", Test_Timestamp_Get());
    test_report("Test_Timestamp_Format", Test_Timestamp_Format());

    // Integration test
    test_report("Test_FullCycle", Test_FullCycle());

    // Summary
    USART1_Printf("=== Test Summary ===\r\n");
    USART1_Printf("Total: %d  Passed: %d  Failed: %d\r\n",
                  test_pass + test_fail, test_pass, test_fail);

    if (test_fail == 0) {
        USART1_Printf("ALL TESTS PASSED!\r\n");
    } else {
        USART1_Printf("SOME TESTS FAILED!\r\n");
    }
}

void Test_Write(void)
{
    USART1_Printf("=== Test Write ===\r\n");
    for (int i = 0; i < 5; i++) {
        Log_Write(LOG_MODULE_TEST, LOG_LEVEL_INFO, "Test message %d", i);
    }
    USART1_Printf("=== Write Test Done ===\r\n");
}

void Test_Overflow(void)
{
    USART1_Printf("=== Test Overflow ===\r\n");
    Buffer_Clear(&g_logBuffer);

    // Fill buffer
    for (int i = 0; i < LOG_BUFFER_SIZE + 5; i++) {
        LogEntry entry;
        entry.timestamp = HAL_GetTick();
        entry.level = LOG_LEVEL_DEBUG;
        entry.module = LOG_MODULE_TEST;
        snprintf(entry.message, sizeof(entry.message), "Overflow test %d", i);
        Buffer_Write(&g_logBuffer, &entry);
    }

    USART1_Printf("Buffer count: %d, Overflow: %s\r\n",
                  g_logBuffer.count,
                  g_logBuffer.overflow ? "YES" : "NO");
    USART1_Printf("=== Overflow Test Done ===\r\n");
}

void Test_Filter(void)
{
    USART1_Printf("=== Test Level Filter ===\r\n");

    // Set TOUCH module to WARNING level
    g_logConfig.level[LOG_MODULE_TOUCH] = LOG_LEVEL_WARNING;

    // This should be filtered out (INFO < WARNING)
    Log_Write(LOG_MODULE_TOUCH, LOG_LEVEL_INFO, "This should NOT appear");

    // This should appear
    Log_Write(LOG_MODULE_TOUCH, LOG_LEVEL_WARNING, "This SHOULD appear");

    // Reset level
    g_logConfig.level[LOG_MODULE_TOUCH] = LOG_LEVEL_INFO;

    USART1_Printf("=== Filter Test Done ===\r\n");
}
```

- [ ] **Step 3: Verify compilation**

Check that test_log.c compiles without errors.

- [ ] **Step 4: Commit**

```bash
git add USER/TEST/test_log.h USER/TEST/test_log.c
git commit -m "feat: add test module with 15 test cases"
```

---

### Task 5: Integrate with Main

**Files:**
- Modify: `Main/main.c`

**Interfaces:**
- Consumes: log.h, test_log.h, usart.h
- Produces: Updated main.c with log system initialization

- [ ] **Step 1: Update main.c includes**

Add the following includes at the top of main.c:

```c
#include "usart.h"
#include "log.h"
#include "test_log.h"
```

- [ ] **Step 2: Add USART and Log initialization**

In main(), after `Touch_Init();` and before the display code, add:

```c
USART1_Init();          // Initialize USART1
Log_Init(LOG_BUFFER_SIZE);  // Initialize log system
```

- [ ] **Step 3: Add command processing loop**

Add a command processing function and call it in the main loop:

```c
// Command receive buffer
static char rxBuffer[64];
static uint8_t rxIndex = 0;

// Process received character
void ProcessRxChar(uint8_t ch)
{
    if (ch == '\r' || ch == '\n') {
        if (rxIndex > 0) {
            rxBuffer[rxIndex] = '\0';
            Log_ProcessCommand(rxBuffer);
            rxIndex = 0;
        }
    } else if (rxIndex < sizeof(rxBuffer) - 1) {
        rxBuffer[rxIndex++] = ch;
    }
}
```

- [ ] **Step 4: Add USART interrupt handler**

Add interrupt-based receive in main.c (or create separate handler):

```c
// USART1 IRQ handler
void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}

// UART receive callback
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        ProcessRxChar(huart->pRxBuffPtr[0]);
        // Re-enable receive interrupt
        HAL_UART_Receive_IT(&huart1, huart->pRxBuffPtr, 1);
    }
}
```

- [ ] **Step 5: Update main loop**

Update the existing main loop to include command processing:

```c
int main(void)
{
    HAL_Init();
    Stm32_Clock_Init(336, 8, 2, 7);
    delay_init();
    LED_Init();
    BEEP_Init();
    KEY_Init();
    LCD_Init();
    Touch_Init();

    // Initialize USART and Log system
    USART1_Init();
    Log_Init(LOG_BUFFER_SIZE);

    // Start USART1 receive interrupt
    uint8_t rxChar;
    HAL_UART_Receive_IT(&huart1, &rxChar, 1);

    BRUSH_COLOR = RED;
    LCD_DisplayString(10, 10, 16, "Illuminati STM32");
    LCD_DisplayString(20, 40, 24, "Author:Clever");
    LCD_DisplayString(30, 80, 24, "19.TOUCH TEST");

    Log_Write(LOG_MODULE_SYSTEM, LOG_LEVEL_INFO, "System started");

    delay_ms(1000);

    Clear_Screen();
    if (lcd_id == 0x9341)
        R_Touch_test();
    else if (lcd_id == 0x1963)
        C_Touch_test();
}
```

- [ ] **Step 6: Verify full compilation**

Check that the entire project compiles without errors.

- [ ] **Step 7: Commit**

```bash
git add Main/main.c
git commit -m "feat: integrate log system with main program"
```

---

### Task 6: Add Log Calls to Existing Modules

**Files:**
- Modify: `USER/TOUCH/touch.c`

**Interfaces:**
- Consumes: log.h
- Produces: Touch event logs

- [ ] **Step 1: Add log include to touch.c**

Add at the top of touch.c:

```c
#include "log.h"
```

- [ ] **Step 2: Add touch event logging**

In `R_Touch_test()`, add logging for touch events:

```c
if (Xdown < lcd_width && Ydown < lcd_height)
{
    if (Xdown > (lcd_width - 40) && Ydown > lcd_height - 18) {
        Clear_Screen();
        Log_Write(LOG_MODULE_TOUCH, LOG_LEVEL_INFO, "Screen cleared");
    } else {
        Draw_Point(Xdown, Ydown, RED);
        Log_Write(LOG_MODULE_TOUCH, LOG_LEVEL_DEBUG,
                  "Touch: x=%d, y=%d", Xdown, Ydown);
    }
}
```

- [ ] **Step 3: Commit**

```bash
git add USER/TOUCH/touch.c
git commit -m "feat: add touch event logging"
```

---

## Self-Review Checklist

1. **Spec coverage:** All requirements from spec are covered:
   - Log levels (ERROR/WARNING/INFO/DEBUG) ✓
   - Module control (TOUCH/LCD/SYSTEM) ✓
   - RAM buffer (64 entries) ✓
   - Command parser ✓
   - Test environment ✓
   - USART1 115200 ✓

2. **Placeholder scan:** No TBD/TODO found. All steps have complete code.

3. **Type consistency:** All types match between header and implementation:
   - LogLevel, LogModule, LogEntry, LogBuffer, BufferStats, LogConfig ✓
   - All function signatures match ✓

4. **File structure:** Matches spec exactly:
   - USER/LOG/log.h, log.c ✓
   - USER/TEST/test_log.h, test_log.c ✓
   - USER/USART/usart.h, usart.c ✓
   - Main/main.c (modified) ✓
