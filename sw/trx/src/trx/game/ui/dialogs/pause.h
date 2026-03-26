#pragma once

// A pause exit confirmation dialog.

#include <trx/game/ui/common.h>
#include <trx/game/ui/elements/requester.h>

typedef struct {
    int32_t phase;
    UI_REQUESTER_STATE req;
} UI_PAUSE_STATE;

typedef enum {
    UI_PAUSE_NOOP,
    UI_PAUSE_RESUME_PAUSE,
    UI_PAUSE_EXIT_TO_GAME,
    UI_PAUSE_EXIT_TO_TITLE,
} UI_PAUSE_EXIT_CHOICE;

// state functions
void UI_Pause_Init(UI_PAUSE_STATE *s);
UI_PAUSE_EXIT_CHOICE UI_Pause_Control(UI_PAUSE_STATE *s);
void UI_Pause_Free(UI_PAUSE_STATE *s);

// draw functions
void UI_Pause(UI_PAUSE_STATE *s);
