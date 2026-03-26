#pragma once

// A controls editor dialog.

#include <trx/core/event_manager.h>
#include <trx/game/input.h>
#include <trx/game/ui/common.h>
#include <trx/game/ui/dialogs/controls_backend.h>
#include <trx/game/ui/dialogs/controls_editor.h>

typedef struct {
    int32_t phase;
    INPUT_BACKEND backend;
    int32_t active_layout;

    EVENT_MANAGER *events;
    UI_CONTROLS_BACKEND_STATE backend_state;
    UI_CONTROLS_EDITOR_STATE editor_state[INPUT_BACKEND_NUMBER_OF];
} UI_CONTROLS_STATE;

// state functions
void UI_Controls_Init(UI_CONTROLS_STATE *s);
void UI_Controls_Free(UI_CONTROLS_STATE *s);
bool UI_Controls_Control(UI_CONTROLS_STATE *s);

// draw functions
void UI_Controls(UI_CONTROLS_STATE *s);
