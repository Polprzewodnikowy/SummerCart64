#pragma once

#include <trx/config/types.h>
#include <trx/core/event_manager.h>
#include <trx/core/shell.h>
#include <trx/game/shell/args.h>

#include <SDL2/SDL_events.h>
#include <stdint.h>

typedef struct {
    int32_t w;
    int32_t h;
} SHELL_SIZE;

void Shell_Shutdown(void);

SDL_Window *Shell_GetWindow(void);
const char *Shell_GetConfigDir(void);
const char *Shell_GetCacheDir(void);

int32_t Shell_Main(const SHELL_ARGS *args);
void Shell_Terminate(int32_t exit_code);

void Shell_ScheduleExit(void);
bool Shell_IsExiting(void);

void Shell_RequestModSwitch(const char *mod_name);
const char *Shell_GetPendingMod(void);
void Shell_ClearPendingMod(void);
bool Shell_GetPrevHeadless(void);
bool Shell_GetPrevQuiet(void);
const SHELL_ARGS *Shell_GetArgs(void);

bool Shell_IsFullscreen(void);
SHELL_SIZE Shell_GetDefaultSize(void);
SHELL_SIZE Shell_GetWindowSize(void);
SHELL_SIZE Shell_GetCurrentSize(void);
SHELL_SIZE Shell_GetCurrentDisplaySize(void);
