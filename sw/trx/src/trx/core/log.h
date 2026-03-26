#pragma once

typedef enum {
    // from least important to most important
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_MAX = -1,
} LOG_LEVEL;

#define LOG_ANSI_COLOR_RED "\x1b[31m"
#define LOG_ANSI_COLOR_GREEN "\x1b[32m"
#define LOG_ANSI_COLOR_YELLOW "\x1b[33m"
#define LOG_ANSI_COLOR_CYAN "\x1b[36m"
#define LOG_ANSI_COLOR_RESET "\x1b[0m"

#define LOG_GENERIC(level, ...)                                                \
    Log_Message(level, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_INFO(...) LOG_GENERIC(LOG_LEVEL_INFO, __VA_ARGS__)
#define LOG_WARNING(...) LOG_GENERIC(LOG_LEVEL_WARNING, __VA_ARGS__)
#define LOG_ERROR(...) LOG_GENERIC(LOG_LEVEL_ERROR, __VA_ARGS__)
#define LOG_DEBUG(...) LOG_GENERIC(LOG_LEVEL_DEBUG, __VA_ARGS__)
#define LOG_TRACE(...)
// disable by default

#define LOG_VAR(var)                                                           \
    _Generic(                                                                  \
        (var),                                                                 \
        int: LOG_DEBUG(#var ": %d", var),                                      \
        bool: LOG_DEBUG(#var ": %d", var),                                     \
        int8_t: LOG_DEBUG(#var ": %d", var),                                   \
        int16_t: LOG_DEBUG(#var ": %d", var),                                  \
        uint8_t: LOG_DEBUG(#var ": %d", var),                                  \
        uint16_t: LOG_DEBUG(#var ": %d", var),                                 \
        uint32_t: LOG_DEBUG(#var ": %d", var),                                 \
        float: LOG_DEBUG(#var ": %f", var),                                    \
        double: LOG_DEBUG(#var ": %f", var),                                   \
        char *: LOG_DEBUG(#var ": %s", var),                                   \
        const char *: LOG_DEBUG(#var ": %s", var),                             \
        default: LOG_DEBUG(#var ": %p", var))

void Log_Init(const char *path, LOG_LEVEL min_level);
LOG_LEVEL Log_GetMinLevel(void);
void Log_SetMinLevel(LOG_LEVEL min_level);
void Log_Shutdown(void);
void Log_Message(
    LOG_LEVEL level, const char *file, int line, const char *func,
    const char *fmt, ...);

// platform-specific implementations
bool Log_ShouldUseAnsiColors(void);
void Log_Init_Extra(const char *path);
void Log_Shutdown_Extra(void);
