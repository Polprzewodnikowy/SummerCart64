#pragma once

#include <trx/game/ui/elements/prompt.h>
#include <trx/game/ui/hud/console_logs.h>

// Dev console display widget.

typedef struct {
    UI_CONSOLE_LOGS logs;
    UI_PROMPT_STATE prompt;

    int32_t listeners[5];
    int32_t history_idx;
} UI_CONSOLE_STATE;

// state functions
void UI_Console_Init(UI_CONSOLE_STATE *s);
void UI_Console_Free(UI_CONSOLE_STATE *s);
void UI_Console_Control(UI_CONSOLE_STATE *s);

// draw functions
void UI_Console(UI_CONSOLE_STATE *s);
