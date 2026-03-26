#include <trx/game/console/common.h>

#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/debug.h>
#include <trx/game/console/internal.h>
#include <trx/game/console/registry.h>
#include <trx/game/game_strings/entries.h>
#include <trx/game/ui.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static bool m_IsOpened = false;
static UI_CONSOLE_STATE m_UIState = {};

// Controls whether console commands emit log events to the UI console
static bool m_Verbose = true;

void Console_Init(void)
{
    UI_Console_Init(&m_UIState);

    Console_History_Init();
}

void Console_Shutdown(void)
{
    UI_Console_Free(&m_UIState);

    Console_History_Shutdown();

    m_IsOpened = false;
}

void Console_Open(void)
{
    if (m_IsOpened) {
        return;
    }
    m_IsOpened = true;
    UI_FireEvent(
        (EVENT) { .name = "console_open", .sender = nullptr, .data = nullptr });
}

void Console_Close(void)
{
    if (!m_IsOpened) {
        return;
    }
    m_IsOpened = false;
    UI_FireEvent((EVENT) {
        .name = "console_close", .sender = nullptr, .data = nullptr });
}

bool Console_IsOpened(void)
{
    return m_IsOpened;
}

void Console_LogEx(
    const LOG_LEVEL level, const char *file, int line, const char *func,
    const char *const fmt, ...)
{
    ASSERT(fmt != nullptr);

    va_list va;
    va_start(va, fmt);
    va_list va_copy;
    va_copy(va_copy, va);

    const size_t text_length = vsnprintf(nullptr, 0, fmt, va);
    char *text = Memory_Alloc(text_length + 1);
    va_end(va);

    vsnprintf(text, text_length + 1, fmt, va_copy);
    va_end(va_copy);

    Log_Message(level, file, line, func, "%s", text);

    if (m_Verbose) {
        UI_FireEvent((EVENT) {
            .name = "console_log",
            .sender = nullptr,
            .data = text,
        });
    }

    Memory_FreePointer(&text);
}

void Console_SetVerbose(const bool verbose)
{
    m_Verbose = verbose;
}

bool Console_IsVerbose(void)
{
    return m_Verbose;
}

void Console_Clear(void)
{
    UI_FireEvent((EVENT) {
        .name = "console_clear",
    });
}

COMMAND_RESULT Console_Eval(const char *const cmdline)
{
    LOG_INFO("executing command: %s", cmdline);

    const CONSOLE_COMMAND *const matching_cmd = Console_Registry_Get(cmdline);
    if (matching_cmd == nullptr) {
        Console_LogError(GS("general/osd/unknown_command"), cmdline);
        return CR_BAD_INVOCATION;
    }

    char *prefix = Memory_DupStr(cmdline);
    char *args = "";
    char *space = strchr(prefix, ' ');
    if (space != nullptr) {
        *space = '\0';
        args = space + 1;
    }

    const COMMAND_CONTEXT ctx = {
        .cmd = matching_cmd,
        .prefix = prefix,
        .args = args,
    };
    ASSERT(matching_cmd->proc != nullptr);
    const COMMAND_RESULT result = matching_cmd->proc(&ctx);
    Memory_FreePointer(&prefix);

    switch (result) {
    case CR_BAD_INVOCATION:
        Console_LogError(GS("general/osd/command_bad_invocation"), cmdline);
        break;

    case CR_UNAVAILABLE:
        Console_LogError(GS("general/osd/command_unavailable"));
        break;

    case CR_SUCCESS:
    case CR_FAILURE:
        // The commands themselves are responsible for handling logging in
        // these scenarios.
        break;
    }
    return result;
}

void Console_Control(void)
{
    UI_Console_Control(&m_UIState);
}

void Console_Draw(void)
{
    UI_Console(&m_UIState);
}
