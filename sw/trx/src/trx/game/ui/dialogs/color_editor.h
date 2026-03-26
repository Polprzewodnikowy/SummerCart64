#pragma once

#include <trx/game/ui/dialogs/settings.h>

typedef struct UI_COLOR_EDITOR_DIALOG_STATE UI_COLOR_EDITOR_DIALOG_STATE;

UI_COLOR_EDITOR_DIALOG_STATE *UI_ColorEditorDialog_Init(void);
void UI_ColorEditorDialog_Free(UI_COLOR_EDITOR_DIALOG_STATE *s);

void UI_ColorEditorDialog_Open(
    UI_COLOR_EDITOR_DIALOG_STATE *s, const UI_SETTINGS_OPTION *option);
void UI_ColorEditorDialog_Close(UI_COLOR_EDITOR_DIALOG_STATE *s);
bool UI_ColorEditorDialog_IsOpen(const UI_COLOR_EDITOR_DIALOG_STATE *s);

void UI_ColorEditorDialog_Control(UI_COLOR_EDITOR_DIALOG_STATE *s);
void UI_ColorEditorDialog(UI_COLOR_EDITOR_DIALOG_STATE *s);
