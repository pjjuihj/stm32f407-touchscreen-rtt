#ifndef TEST_CONSOLE_LOG_H
#define TEST_CONSOLE_LOG_H

#include <stdbool.h>

typedef enum {
    LOG_MODULE_TOUCH = 0,
    LOG_MODULE_LCD,
    LOG_MODULE_SYSTEM,
    LOG_MODULE_MAX
} LogModule;

typedef enum {
    LOG_LEVEL_ERROR = 1,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG
} LogLevel;

typedef struct {
    bool enabled[LOG_MODULE_MAX];
} LogConfig;

extern const char *const LogModuleNames[LOG_MODULE_MAX];
extern LogConfig g_logConfig;

void Log_Enable(LogModule module, bool enabled);
void Log_SetLevel(LogModule module, LogLevel level);
void Log_Write(LogModule module, LogLevel level, const char *format, ...);
void Log_Dump(void);
void Log_Clear(void);

#endif
