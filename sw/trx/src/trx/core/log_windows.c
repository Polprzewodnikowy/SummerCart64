#include <trx/core/log.h>

#include <trx/core/memory.h>

#include <windows.h>
#include <dbghelp.h>
#include <tlhelp32.h>
#include <dwarfstack.h>
#include <io.h>
#include <process.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
    #define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

static char *m_MiniDumpPath = nullptr;

static char *M_GetMiniDumpPath(const char *const log_path)
{
    char *dot = strrchr(log_path, '.');
    if (dot == nullptr) {
        return nullptr;
    }

    const size_t index = dot - log_path;
    const char *new_extension = ".dmp";
    const size_t new_len = index + strlen(new_extension) + 1;
    char *minidump_path = Memory_Alloc(new_len);
    strncpy(minidump_path, log_path, index);
    strcat(minidump_path, new_extension);
    return minidump_path;
}

static void M_StackTrace(
    const uint64_t addr, const char *filename, const int line_no,
    const char *const func_name, void *const context, const int column_no)
{
    int32_t *count = context;
    void *ptr = (void *)(uintptr_t)addr;

    switch (line_no) {
    case DWST_BASE_ADDR:
        LOG_INFO("--- 0x%p: %s", ptr, filename);
        break;

    case DWST_NOT_FOUND:
    case DWST_NO_DBG_SYM:
    case DWST_NO_SRC_FILE:
        LOG_INFO("%02d. 0x%p: %s", *count, ptr, filename);
        (*count)++;
        break;

    default:
        if (ptr != nullptr) {
            LOG_INFO(
                "%02d. 0x%p: (%s:%d:%d) %s", *count, ptr, filename, line_no,
                column_no, func_name);
        } else {
            LOG_INFO(
                "%02d. %*s (%s:%d:%d) %s", *count, (int32_t)sizeof(void *) * 2,
                "", filename, line_no, column_no, func_name);
        }
        (*count)++;
        break;
    }
}

static void M_CreateMiniDump(
    EXCEPTION_POINTERS *const ex, const char *const path)
{
    HANDLE handle = CreateFile(
        path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
        nullptr);
    MINIDUMP_EXCEPTION_INFORMATION dump_info;
    dump_info.ExceptionPointers = ex;
    dump_info.ThreadId = GetCurrentThreadId();
    dump_info.ClientPointers = TRUE;
    MiniDumpWriteDump(
        GetCurrentProcess(), GetCurrentProcessId(), handle, MiniDumpNormal,
        &dump_info, nullptr, nullptr);
    CloseHandle(handle);

    LOG_INFO("Crash dump info put in %s", path);
}

LONG WINAPI Log_CrashHandler(EXCEPTION_POINTERS *ex)
{
    LOG_ERROR("== CRASH REPORT ==");
    LOG_INFO("EXCEPTION CODE: %x", ex->ExceptionRecord->ExceptionCode);
    LOG_INFO("EXCEPTION ADDRESS: %x", ex->ExceptionRecord->ExceptionAddress);
    LOG_INFO("STACK TRACE:");

    int32_t count = 0;
    dwstOfException(ex->ContextRecord, &M_StackTrace, &count);

    M_CreateMiniDump(ex, m_MiniDumpPath);

    return EXCEPTION_EXECUTE_HANDLER;
}

void Log_Init_Extra(const char *log_path)
{
    // let the game (a gui app) log output to a terminal
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        FILE *fp;
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);
        freopen_s(&fp, "CONIN$", "r", stdin);
    }

    // enable ANSI escape codes processing
    HANDLE h_out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h_out != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(h_out, &mode)) {
            SetConsoleMode(h_out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }

    if (log_path != nullptr) {
        m_MiniDumpPath = M_GetMiniDumpPath(log_path);
        SetUnhandledExceptionFilter(Log_CrashHandler);
    }
}

void Log_Shutdown_Extra(void)
{
    Memory_FreePointer(&m_MiniDumpPath);
}

bool Log_ShouldUseAnsiColors(void)
{
    return _isatty(_fileno(stdout));
}
