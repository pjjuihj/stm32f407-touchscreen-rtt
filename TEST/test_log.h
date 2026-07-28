#ifndef __TEST_LOG_H
#define __TEST_LOG_H

#include "log.h"

// Test result structure
typedef struct {
    const char *name;
    bool passed;
    char message[64];
} TestResult;

// Run all tests
void Test_RunAll(void);

// Individual tests
TestResult Test_Log_Init(void);
TestResult Test_Log_Write(void);
TestResult Test_Log_Enable(void);
TestResult Test_Log_SetLevel(void);
TestResult Test_Buffer_Clear(void);
TestResult Test_Buffer_Write(void);
TestResult Test_Buffer_Overflow(void);
TestResult Test_Timestamp(void);

#endif
