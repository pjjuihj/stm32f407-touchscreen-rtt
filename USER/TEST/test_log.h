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
