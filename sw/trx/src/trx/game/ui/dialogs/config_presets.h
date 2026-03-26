#pragma once

// A UI dialog for browsing and applying config presets.
// Shows a scrollable list of presets. Selecting one prompts the user
// with a list of settings that will change, then applies them on confirm.

#include <trx/game/ui/scrollable.h>

typedef struct UI_CONFIG_PRESETS_STATE UI_CONFIG_PRESETS_STATE;

// State functions
UI_CONFIG_PRESETS_STATE *UI_ConfigPresets_Init(void);
void UI_ConfigPresets_Free(UI_CONFIG_PRESETS_STATE *s);
int32_t UI_ConfigPresets_GetItemCount(UI_CONFIG_PRESETS_STATE *s);
void UI_ConfigPresets_RecomputeSizes(
    UI_CONFIG_PRESETS_STATE *s, int32_t visible_rows);
float UI_ConfigPresets_GetContentWidth(UI_CONFIG_PRESETS_STATE *s);
float UI_ConfigPresets_GetContentHeight(UI_CONFIG_PRESETS_STATE *s);

// Handle input. Returns true when the user wants to exit the dialog.
bool UI_ConfigPresets_Control(UI_CONFIG_PRESETS_STATE *s);
UI_SCROLLABLE *UI_ConfigPresets_GetScrollable(UI_CONFIG_PRESETS_STATE *s);

// Draw functions
void UI_ConfigPresets(UI_CONFIG_PRESETS_STATE *s);
void UI_ConfigPresetsApplyModal(UI_CONFIG_PRESETS_STATE *s);
