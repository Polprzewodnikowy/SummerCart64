// UI dialog for adjusting music and sound volumes
#pragma once

#include <trx/game/ui/common.h>
#include <trx/game/ui/dialogs/settings.h>

// Initialize the sound settings dialog state.
UI_SETTINGS_DIALOG_STATE *UI_SoundSettings_Init(void);

// Free resources used by the sound settings dialog.
void UI_SoundSettings_Free(UI_SETTINGS_DIALOG_STATE *s);

// Handle input/control for the sound settings dialog.
// Returns true if the dialog should be closed.
bool UI_SoundSettings_Control(UI_SETTINGS_DIALOG_STATE *s);

// Render the sound settings dialog.
void UI_SoundSettings(UI_SETTINGS_DIALOG_STATE *s);
