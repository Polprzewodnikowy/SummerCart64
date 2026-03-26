#pragma once

// A controls remapper dialog.

#include <trx/core/event_manager.h>
#include <trx/game/game_strings/entries.h>
#include <trx/game/input.h>
#include <trx/game/ui/common.h>
#include <trx/game/ui/elements/flash.h>
#include <trx/game/ui/elements/progress_button.h>
#include <trx/game/ui/elements/requester.h>
#include <trx/game/ui/elements/tab_switch.h>
#include <trx/game/ui/scrollable.h>

typedef struct {
    INPUT_ROLE role;
} UI_CONTROLS_EDITOR_ROW;

typedef struct {
    GAME_STRING_ID header_gs;
    UI_CONTROLS_EDITOR_ROW *rows;
} UI_CONTROLS_EDITOR_GROUP;

typedef struct {
    int32_t phase;
    INPUT_BACKEND backend;
    INPUT_LAYOUT active_layout;
    INPUT_ROLE active_role;
    const UI_CONTROLS_EDITOR_GROUP *active_group;
    UI_FLASH_STATE flash;
    EVENT_MANAGER *events;

    UI_SCROLLABLE scroll;

    int32_t max_group_items;
    int32_t input_size;
    int32_t label_size;
    UI_TAB_SWITCH_STATE *layout_tab_switch;
    UI_TAB_SWITCH_STATE *controls_tab_switch;
    UI_PROGRESS_BUTTON_STATE *reset_bindings_button;
    UI_PROGRESS_BUTTON_STATE *unbind_key_button;
} UI_CONTROLS_EDITOR_STATE;

typedef enum {
    UI_CONTROLS_CHOICE_EXIT,
    UI_CONTROLS_CHOICE_GO_BACK,
    UI_CONTROLS_CHOICE_NOOP,
} UI_CONTROLS_CHOICE;

// state functions
void UI_ControlsEditor_Init(
    UI_CONTROLS_EDITOR_STATE *s, INPUT_BACKEND backend, int32_t layout,
    EVENT_MANAGER *events);
void UI_ControlsEditor_Free(UI_CONTROLS_EDITOR_STATE *s);
UI_CONTROLS_CHOICE UI_ControlsEditor_Control(UI_CONTROLS_EDITOR_STATE *s);

// draw functions
void UI_ControlsEditor(UI_CONTROLS_EDITOR_STATE *s);
