#pragma once

// A mod selector dialog used by the passport.

#include <trx/game/ui/common.h>

typedef struct UI_SWITCH_MOD_DIALOG_STATE UI_SWITCH_MOD_DIALOG_STATE;

// state functions
UI_SWITCH_MOD_DIALOG_STATE *UI_SwitchModDialog_Init(void);
void UI_SwitchModDialog_Free(UI_SWITCH_MOD_DIALOG_STATE *s);
int32_t UI_SwitchModDialog_Control(UI_SWITCH_MOD_DIALOG_STATE *s);
const char *UI_SwitchModDialog_GetSelectedMod(
    const UI_SWITCH_MOD_DIALOG_STATE *s, int32_t choice);

// draw functions
void UI_SwitchModDialog(UI_SWITCH_MOD_DIALOG_STATE *s);
