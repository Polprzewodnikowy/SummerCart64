#include <trx/core/log.h>

#include <stdarg.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

#define M_FORMAT "%s | %s [%s:%d:%s] "

static LOG_LEVEL m_LogLevel = LOG_LEVEL_MAX;
static FILE *m_LogHandle = nullptr;
static bool m_UseAnsiColors = true;

static const char *const m_LogLevelColors[] = {
    [LOG_LEVEL_INFO] = LOG_ANSI_COLOR_RESET,
    [LOG_LEVEL_WARNING] = LOG_ANSI_COLOR_YELLOW,
    [LOG_LEVEL_ERROR] = LOG_ANSI_COLOR_RED,
    [LOG_LEVEL_DEBUG] = LOG_ANSI_COLOR_CYAN,
};
static const char *const m_LogLevelStrings[] = {
    [LOG_LEVEL_INFO] = "INF",
    [LOG_LEVEL_WARNING] = "WRN",
    [LOG_LEVEL_ERROR] = "ERR",
    [LOG_LEVEL_DEBUG] = "DBG",
};

void Log_Init(const char *path, const LOG_LEVEL min_level)
{
    if (m_LogHandle != nullptr) {
        fclose(m_LogHandle);
        m_LogHandle = nullptr;
    }
    m_LogLevel = min_level;
    m_UseAnsiColors = Log_ShouldUseAnsiColors();
    if (path != nullptr) {
        m_LogHandle = fopen(path, "w");
    }
    Log_Init_Extra(path);
}

LOG_LEVEL Log_GetMinLevel(void)
{
    return m_LogLevel;
}

void Log_SetMinLevel(const LOG_LEVEL min_level)
{
    m_LogLevel = min_level;
}

void Log_Message(
    const LOG_LEVEL level, const char *const file, const int line,
    const char *const func, const char *const fmt, ...)
{
    va_list va;
    va_start(va, fmt);

    char timestamp_str[32];
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    struct tm *const tm_info = localtime(&tv.tv_sec);
    const size_t timestamp_len = strftime(
        timestamp_str, sizeof(timestamp_str), "%Y-%m-%d %H:%M:%S", tm_info);
    snprintf(
        timestamp_str + timestamp_len, sizeof(timestamp_str) - timestamp_len,
        ".%03d", (int)(tv.tv_usec / 1000));

    const char *const log_str = m_LogLevelStrings[level];
    const char *const log_color = m_LogLevelColors[level];

    // print to log file
    if (m_LogHandle != nullptr) {
        va_list vb;

        va_copy(vb, va);
        fprintf(
            m_LogHandle, M_FORMAT, log_str, timestamp_str, file, line, func);
        vfprintf(m_LogHandle, fmt, vb);
        fprintf(m_LogHandle, "\n");
        fflush(m_LogHandle);

        va_end(vb);
    }

    // print to stdout
    if (level >= m_LogLevel) {
        if (m_UseAnsiColors) {
            printf("%s", log_color);
        }
        printf(M_FORMAT, log_str, timestamp_str, file, line, func);
        vprintf(fmt, va);
        if (m_UseAnsiColors) {
            printf("%s", LOG_ANSI_COLOR_RESET);
        }
        printf("\n");
        fflush(stdout);
    }

    va_end(va);
}

void Log_Shutdown(void)
{
    Log_Shutdown_Extra();
    if (m_LogHandle != nullptr) {
        fclose(m_LogHandle);
        m_LogHandle = nullptr;
    }
}
