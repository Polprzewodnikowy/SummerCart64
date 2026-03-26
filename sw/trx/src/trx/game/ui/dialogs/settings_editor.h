#pragma once

#include <trx/game/ui/dialogs/settings.h>

typedef struct UI_SETTINGS_EDITOR_STATE UI_SETTINGS_EDITOR_STATE;

UI_SETTINGS_EDITOR_STATE *UI_SettingsEditor_Init(
    const UI_SETTINGS_OPTION *options);
void UI_SettingsEditor_Free(UI_SETTINGS_EDITOR_STATE *s);

void UI_SettingsEditor_RecomputeSizes(
    UI_SETTINGS_EDITOR_STATE *s, int32_t visible_rows);
UI_SCROLLABLE *UI_SettingsEditor_GetScrollable(UI_SETTINGS_EDITOR_STATE *s);

bool UI_SettingsEditor_Control(
    UI_SETTINGS_EDITOR_STATE *s, UI_SETTINGS_PHASE *dialog_phase);

void UI_SettingsEditor_Draw(
    UI_SETTINGS_EDITOR_STATE *s, const UI_SCROLLABLE *dialog_scroll,
    UI_SETTINGS_PHASE dialog_phase, float row_width);
void UI_SettingsEditor_DrawOverlay(UI_SETTINGS_EDITOR_STATE *s);
void UI_SettingsEditor_DrawFooter(
    UI_SETTINGS_EDITOR_STATE *s, UI_SETTINGS_PHASE dialog_phase);

float UI_SettingsEditor_GetContentWidth(const UI_SETTINGS_EDITOR_STATE *s);
int32_t UI_SettingsEditor_GetItemCount(const UI_SETTINGS_EDITOR_STATE *s);
void UI_SettingsEditor_RequestChange(
    const UI_SETTINGS_OPTION *option, int32_t dir);
