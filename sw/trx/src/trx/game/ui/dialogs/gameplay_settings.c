#include <trx/game/ui/dialogs/gameplay_settings.h>

#include <trx/config.h>
#include <trx/game/lara/const.h>
#include <trx/game/ui/dialogs/setting_helpers/enums.h>
#include <trx/game/ui/dialogs/setting_helpers/handlers.h>
#include <trx/game/ui/dialogs/settings_tabs.h>

static const UI_SETTINGS_OPTION m_GeneralOptions[] = {
#include <trx/game/ui/dialogs/setting_tabs/gameplay_general.def>
    { .target = nullptr },
};

static const UI_SETTINGS_OPTION m_ControlOptions[] = {
#include <trx/game/ui/dialogs/setting_tabs/gameplay_controls.def>
    { .target = nullptr },
};

static const UI_SETTINGS_OPTION m_GameplayModOptions[] = {
#include <trx/game/ui/dialogs/setting_tabs/gameplay_mods.def>
    { .target = nullptr },
};

static const UI_SETTINGS_OPTION m_GameplayFixOptions[] = {
#include <trx/game/ui/dialogs/setting_tabs/gameplay_fixes.def>
    { .target = nullptr },
};

UI_SETTINGS_DIALOG_STATE *UI_GameplaySettings_Init(void)
{
    const UI_SETTINGS_TAB tabs[] = {
        UI_SettingsTab_MakeEditor(
            GS_ID("general/settings/gameplay/tabs/general"), m_GeneralOptions),
        UI_SettingsTab_MakeEditor(
            GS_ID("general/settings/gameplay/tabs/controls"), m_ControlOptions),
        UI_SettingsTab_MakeEditor(
            GS_ID("general/settings/gameplay/tabs/mods"), m_GameplayModOptions),
        UI_SettingsTab_MakeEditor(
            GS_ID("general/settings/gameplay/tabs/fixes"),
            m_GameplayFixOptions),
        UI_SettingsTab_MakePresets(
            GS_ID("general/settings/gameplay/tabs/presets")),
    };

    return UI_SettingsDialog_Init(
        GS_ID("general/settings/gameplay/title"), ARRAY_SIZE(tabs), tabs);
}

void UI_GameplaySettings_Free(UI_SETTINGS_DIALOG_STATE *const s)
{
    UI_SettingsDialog_Free(s);
}

bool UI_GameplaySettings_Control(UI_SETTINGS_DIALOG_STATE *const s)
{
    return UI_SettingsDialog_Control(s);
}

void UI_GameplaySettings(UI_SETTINGS_DIALOG_STATE *const s)
{
    UI_SettingsDialog(s);
}
