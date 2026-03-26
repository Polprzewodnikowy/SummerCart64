#pragma once

#include <trx/game/ui/common.h>
#include <trx/game/ui/dialogs/settings.h>

UI_SETTINGS_DIALOG_STATE *UI_GameplaySettings_Init(void);
void UI_GameplaySettings_Free(UI_SETTINGS_DIALOG_STATE *s);
bool UI_GameplaySettings_Control(UI_SETTINGS_DIALOG_STATE *s);

void UI_GameplaySettings(UI_SETTINGS_DIALOG_STATE *s);
