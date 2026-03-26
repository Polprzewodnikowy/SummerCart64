#pragma once

#include <trx/game/ui/common.h>
#include <trx/game/ui/dialogs/settings.h>

UI_SETTINGS_DIALOG_STATE *UI_GraphicSettings_Init(void);
void UI_GraphicSettings_Free(UI_SETTINGS_DIALOG_STATE *s);
bool UI_GraphicSettings_Control(UI_SETTINGS_DIALOG_STATE *s);

void UI_GraphicSettings(UI_SETTINGS_DIALOG_STATE *s);
