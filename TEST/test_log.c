#include "test_log.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>

// Helper function to create test result
static TestResult make_result(const char *name, bool passed, const char *msg) {
    TestResult r;
    r.name = name;
    r.passed = passed;
    strncpy(r.message, msg, sizeof(r.message) - 1);
    r.message[sizeof(r.message) - 1] = '\0';
    return r;
}

// Test 1: Log_Init
TestResult Test_Log_Init(void) {
    Log_Init();
    // Check if all modules are enabled
    for (int i = 0; i < LOG_MODULE_MAX; i++) {
        if (!g_logConfig.enabled[i]) {
            return make_result("Test_Log_Init", false, "Module not enabled");
        }
    }
    // Check buffer is empty
    if (g_logBuffer.count != 0) {
        return make_result("Test_Log_Init", false, "Buffer not empty");
    }
    return make_result("Test_Log_Init", true, "OK");
}

// Test 2: Log_Write
TestResult Test_Log_Write(void) {
    Log_Init();
    uint16_t count_before = g_logBuffer.count;
    Log_Write(LOG_MODULE_SYSTEM, LOG_LEVEL_INFO, "Test message");
    if (g_logBuffer.count != count_before + 1) {
        return make_result("Test_Log_Write", false, "Buffer count not incremented");
    }
    // Check the entry
    LogEntry *e = &g_logBuffer.entries[g_logBuffer.tail];
    if (e->module != LOG_MODULE_SYSTEM) {
        return make_result("Test_Log_Write", false, "Wrong module");
    }
    if (e->level != LOG_LEVEL_INFO) {
        return make_result("Test_Log_Write", false, "Wrong level");
    }
    if (strcmp(e->message, "Test message") != 0) {
        return make_result("Test_Log_Write", false, "Wrong message");
    }
    return make_result("Test_Log_Write", true, "OK");
}

// Test 3: Log_Enable
TestResult Test_Log_Enable(void) {
    Log_Init();
    // Disable TOUCH module
    Log_Enable(LOG_MODULE_TOUCH, false);
    if (g_logConfig.enabled[LOG_MODULE_TOUCH]) {
        return make_result("Test_Log_Enable", false, "Module not disabled");
    }
    // Re-enable
    Log_Enable(LOG_MODULE_TOUCH, true);
    if (!g_logConfig.enabled[LOG_MODULE_TOUCH]) {
        return make_result("Test_Log_Enable", false, "Module not re-enabled");
    }
    return make_result("Test_Log_Enable", true, "OK");
}

// Test 4: Log_SetLevel
TestResult Test_Log_SetLevel(void) {
    Log_Init();
    // Set TOUCH level to DEBUG
    Log_SetLevel(LOG_MODULE_TOUCH, LOG_LEVEL_DEBUG);
    if (g_logConfig.level[LOG_MODULE_TOUCH] != LOG_LEVEL_DEBUG) {
        return make_result("Test_Log_SetLevel", false, "Level not set");
    }
    // Reset to INFO
    Log_SetLevel(LOG_MODULE_TOUCH, LOG_LEVEL_INFO);
    if (g_logConfig.level[LOG_MODULE_TOUCH] != LOG_LEVEL_INFO) {
        return make_result("Test_Log_SetLevel", false, "Level not reset");
    }
    return make_result("Test_Log_SetLevel", true, "OK");
}

// Test 5: Buffer Clear
TestResult Test_Buffer_Clear(void) {
    Log_Init();
    // Add some entries
    for (int i = 0; i < 5; i++) {
        Log_Write(LOG_MODULE_SYSTEM, LOG_LEVEL_INFO, "Test %d", i);
    }
    // Clear
    Log_Clear();
    if (g_logBuffer.count != 0) {
        return make_result("Test_Buffer_Clear", false, "Buffer not cleared");
    }
    if (g_logBuffer.head != 0 || g_logBuffer.tail != 0) {
        return make_result("Test_Buffer_Clear", false, "Head/tail not reset");
    }
    return make_result("Test_Buffer_Clear", true, "OK");
}

// Test 6: Buffer Write
TestResult Test_Buffer_Write(void) {
    Log_Init();
    // Fill buffer partially
    for (int i = 0; i < 10; i++) {
        Log_Write(LOG_MODULE_SYSTEM, LOG_LEVEL_INFO, "Test %d", i);
    }
    if (g_logBuffer.count != 10) {
        return make_result("Test_Buffer_Write", false, "Wrong count");
    }
    return make_result("Test_Buffer_Write", true, "OK");
}

// Test 7: Buffer Overflow
TestResult Test_Buffer_Overflow(void) {
    Log_Init();
    // Fill buffer completely
    for (int i = 0; i < LOG_BUFFER_SIZE; i++) {
        Log_Write(LOG_MODULE_SYSTEM, LOG_LEVEL_INFO, "Test %d", i);
    }
    if (g_logBuffer.count != LOG_BUFFER_SIZE) {
        return make_result("Test_Buffer_Overflow", false, "Buffer not full");
    }
    // Write one more - should overflow
    Log_Write(LOG_MODULE_SYSTEM, LOG_LEVEL_INFO, "Overflow test");
    if (g_logBuffer.count != LOG_BUFFER_SIZE) {
        return make_result("Test_Buffer_Overflow", false, "Count wrong after overflow");
    }
    if (!g_logBuffer.overflow) {
        return make_result("Test_Buffer_Overflow", false, "Overflow flag not set");
    }
    return make_result("Test_Buffer_Overflow", true, "OK");
}

// Test 8: Timestamp
TestResult Test_Timestamp(void) {
    uint32_t t1 = HAL_GetTick();
    for (volatile int i = 0; i < 10000; i++);  // Small delay
    uint32_t t2 = HAL_GetTick();
    if (t2 < t1) {
        return make_result("Test_Timestamp", false, "Timestamp went backwards");
    }
    return make_result("Test_Timestamp", true, "OK");
}

// Run all tests
void Test_RunAll(void) {
    TestResult results[8];
    int pass = 0, fail = 0;

    USART1_SendString("\r\n=== Running All Tests ===\r\n");

    results[0] = Test_Log_Init();
    results[1] = Test_Log_Write();
    results[2] = Test_Log_Enable();
    results[3] = Test_Log_SetLevel();
    results[4] = Test_Buffer_Clear();
    results[5] = Test_Buffer_Write();
    results[6] = Test_Buffer_Overflow();
    results[7] = Test_Timestamp();

    for (int i = 0; i < 8; i++) {
        char buf[64];
        if (results[i].passed) {
            snprintf(buf, sizeof(buf), "[PASS] %s\r\n", results[i].name);
            pass++;
        } else {
            snprintf(buf, sizeof(buf), "[FAIL] %s - %s\r\n", results[i].name, results[i].message);
            fail++;
        }
        USART1_SendString(buf);
    }

    char summary[64];
    snprintf(summary, sizeof(summary), "\r\nTotal: %d  Pass: %d  Fail: %d\r\n", pass + fail, pass, fail);
    USART1_SendString(summary);
}
