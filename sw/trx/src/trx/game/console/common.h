#pragma once

#include <trx/core/log.h>
#include <trx/game/console/types.h>

#include <stdint.h>

#define Console_LogGeneric(level, ...)                                         \
    Console_LogEx(level, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define Console_Log(...) Console_LogGeneric(LOG_LEVEL_INFO, __VA_ARGS__)
#define Console_LogWarning(...)                                                \
    Console_LogGeneric(LOG_LEVEL_WARNING, __VA_ARGS__)
#define Console_LogError(...) Console_LogGeneric(LOG_LEVEL_ERROR, __VA_ARGS__)

void Console_Init(void);
void Console_Shutdown(void);

void Console_Open(void);
void Console_Close(void);
bool Console_IsOpened(void);

void Console_LogEx(
    LOG_LEVEL level, const char *file, int line, const char *func,
    const char *fmt, ...);
void Console_Clear(void);
COMMAND_RESULT Console_Eval(const char *cmdline);

// Controls whether console commands emit log events to the UI console
void Console_SetVerbose(bool verbose);
bool Console_IsVerbose(void);

void Console_Control(void);
void Console_Draw(void);
