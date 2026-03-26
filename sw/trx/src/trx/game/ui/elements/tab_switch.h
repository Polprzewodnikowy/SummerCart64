#pragma once

// A tab switch UI element for navigating between multiple tabs via left/right
// input.

#include <trx/game/game_strings/entries.h>
#include <trx/game/ui/common.h>
#include <trx/game/ui/scrollable.h>

// Represents a single tab page for use with UI_TabSwitch_Control.
typedef struct {
    struct {
        const char *one_off;
        const char *const *live_ptr;
    } header;
} UI_TAB_SWITCH_TAB;

typedef struct {
    UI_TAB_SWITCH_TAB *tabs;
    int32_t tab_count;
    int32_t active_tab_idx;
} UI_TAB_SWITCH_STATE;

typedef enum {
    UI_TAB_SWITCH_NORMAL,
    UI_TAB_SWITCH_NO_ARROWS,
} UI_TAB_SWITCH_FLAGS;

// state functions
UI_TAB_SWITCH_STATE *UI_TabSwitch_Init(
    int32_t tab_count, const UI_TAB_SWITCH_TAB *tabs);
void UI_TabSwitch_Free(UI_TAB_SWITCH_STATE *s);

// Handles left/right input for switching tabs. Returns true if the active tab
// changed.
bool UI_TabSwitch_Control(
    UI_TAB_SWITCH_STATE *state, UI_TAB_SWITCH_FLAGS flags);

// Advances the active tab by dir (-1 for previous, +1 for next), wrapping
// around.
bool UI_TabSwitch_Cycle(UI_TAB_SWITCH_STATE *state, int32_t dir);

// draw functions
void UI_TabSwitch(const UI_TAB_SWITCH_STATE *state, bool is_focused);
void UI_TabSwitchSingle(const UI_TAB_SWITCH_STATE *state, bool is_focused);
