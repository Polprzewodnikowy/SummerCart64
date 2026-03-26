#include <trx/game/ui/dialogs/sound_settings.h>

#include <trx/config.h>
#include <trx/game/lara/const.h>
#include <trx/game/ui/dialogs/setting_helpers/enums.h>
#include <trx/game/ui/dialogs/setting_helpers/handlers.h>
#include <trx/game/ui/dialogs/settings_tabs.h>

static const UI_SETTINGS_OPTION m_SoundVolumeOptions[] = {
#include <trx/game/ui/dialogs/setting_tabs/sound_volume.def>
    { .target = nullptr },
};

static const UI_SETTINGS_OPTION m_SoundMiscOptions[] = {
#include <trx/game/ui/dialogs/setting_tabs/sound_misc.def>
    { .target = nullptr },
};

UI_SETTINGS_DIALOG_STATE *UI_SoundSettings_Init(void)
{
    const UI_SETTINGS_TAB tabs[] = {
        UI_SettingsTab_MakeEditor(
            GS_ID("general/settings/sound/tabs/volume"), m_SoundVolumeOptions),
        UI_SettingsTab_MakeEditor(
            GS_ID("general/settings/sound/tabs/misc"), m_SoundMiscOptions),
    };

    return UI_SettingsDialog_Init(
        GS_ID("general/settings/sound/title"), ARRAY_SIZE(tabs), tabs);
}

void UI_SoundSettings_Free(UI_SETTINGS_DIALOG_STATE *const s)
{
    UI_SettingsDialog_Free(s);
}

bool UI_SoundSettings_Control(UI_SETTINGS_DIALOG_STATE *const s)
{
    return UI_SettingsDialog_Control(s);
}

void UI_SoundSettings(UI_SETTINGS_DIALOG_STATE *const s)
{
    UI_SettingsDialog(s);
}
