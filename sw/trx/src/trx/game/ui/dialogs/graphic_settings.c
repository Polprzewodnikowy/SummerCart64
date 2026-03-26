#include <trx/game/ui/dialogs/graphic_settings.h>

#include <trx/config.h>
#include <trx/game/ui/dialogs/setting_helpers/enums.h>
#include <trx/game/ui/dialogs/setting_helpers/handlers.h>
#include <trx/game/ui/dialogs/settings_tabs.h>

static const UI_SETTINGS_OPTION m_VisualsOptions[] = {
#include <trx/game/ui/dialogs/setting_tabs/graphic_visuals.def>
    { .target = nullptr },
};

static UI_SETTINGS_OPTION m_UIOptions[] = {
#include <trx/game/ui/dialogs/setting_tabs/graphic_ui.def>
    { .target = nullptr },
};

static const UI_SETTINGS_OPTION m_UIBarsOptions[] = {
#include <trx/game/ui/dialogs/setting_tabs/graphic_ui_bars.def>
    { .target = nullptr },
};

static const UI_SETTINGS_OPTION m_RenderOptions[] = {
#include <trx/game/ui/dialogs/setting_tabs/graphic_rendering.def>
    { .target = nullptr },
};

UI_SETTINGS_DIALOG_STATE *UI_GraphicSettings_Init(void)
{
    const UI_SETTINGS_TAB tabs[] = {
        UI_SettingsTab_MakeEditor(
            GS_ID("general/settings/graphic_settings/tabs/visuals"),
            m_VisualsOptions),
        UI_SettingsTab_MakeEditor(
            GS_ID("general/settings/graphic_settings/tabs/ui"), m_UIOptions),
        UI_SettingsTab_MakeEditor(
            GS_ID("general/settings/graphic_settings/tabs/bars"),
            m_UIBarsOptions),
        UI_SettingsTab_MakeEditor(
            GS_ID("general/settings/graphic_settings/tabs/rendering"),
            m_RenderOptions),
    };

    return UI_SettingsDialog_Init(
        GS_ID("general/settings/graphic_settings/title"), ARRAY_SIZE(tabs),
        tabs);
}

void UI_GraphicSettings_Free(UI_SETTINGS_DIALOG_STATE *const s)
{
    UI_SettingsDialog_Free(s);
}

bool UI_GraphicSettings_Control(UI_SETTINGS_DIALOG_STATE *const s)
{
    return UI_SettingsDialog_Control(s);
}

void UI_GraphicSettings(UI_SETTINGS_DIALOG_STATE *const s)
{
    UI_SettingsDialog(s);
}
