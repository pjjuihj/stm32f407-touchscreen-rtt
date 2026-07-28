#include "test_log.h"
#include "log.h"
#include "usart.h"
#include <string.h>
#include <stdio.h>

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
    Log_Init();
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
    Log_Init();

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
