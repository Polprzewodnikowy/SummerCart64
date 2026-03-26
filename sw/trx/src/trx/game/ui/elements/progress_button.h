#pragma once

#include <trx/game/game_strings/entries.h>
#include <trx/game/input.h>
#include <trx/game/ui/common.h>

typedef void (*UI_PROGRESS_BUTTON_CALLBACK)(void *);

typedef struct UI_PROGRESS_BUTTON_STATE UI_PROGRESS_BUTTON_STATE;

UI_PROGRESS_BUTTON_STATE *UI_ProgressButton_Init(
    INPUT_BACKEND backend, INPUT_ROLE role, GAME_STRING_ID text,
    UI_PROGRESS_BUTTON_CALLBACK func, void *func_arg);
void UI_ProgressButton_Control(UI_PROGRESS_BUTTON_STATE *s);
void UI_ProgressButton_Free(UI_PROGRESS_BUTTON_STATE *s);

void UI_ProgressButton(UI_PROGRESS_BUTTON_STATE *s);
