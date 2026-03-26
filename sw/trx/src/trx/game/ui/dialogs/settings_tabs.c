#include <trx/game/ui/dialogs/settings_tabs.h>

#include <trx/game/ui/dialogs/config_presets.h>
#include <trx/game/ui/dialogs/settings_editor.h>

static bool M_EditorControl(
    void *const user_data, UI_SETTINGS_PHASE *const phase)
{
    return UI_SettingsEditor_Control(user_data, phase);
}

static void M_EditorDraw(
    void *const user_data, const UI_SETTINGS_PHASE phase, const float row_width)
{
    UI_SETTINGS_EDITOR_STATE *const editor = user_data;
    UI_SettingsEditor_Draw(
        editor, UI_SettingsEditor_GetScrollable(editor), phase, row_width);
}

static void M_EditorDrawFooter(
    void *const user_data, const UI_SETTINGS_PHASE phase)
{
    UI_SettingsEditor_DrawFooter(user_data, phase);
}

static void M_EditorDrawOverlay(void *const user_data)
{
    UI_SettingsEditor_DrawOverlay(user_data);
}

static void M_EditorFree(void *const user_data)
{
    UI_SettingsEditor_Free(user_data);
}

static UI_SCROLLABLE *M_EditorGetScrollable(void *const user_data)
{
    return UI_SettingsEditor_GetScrollable(user_data);
}

static void M_EditorRecompute(void *const user_data, const int32_t visible_rows)
{
    UI_SettingsEditor_RecomputeSizes(user_data, visible_rows);
}

static float M_EditorGetContentWidth(void *const user_data)
{
    return UI_SettingsEditor_GetContentWidth(user_data);
}

static int32_t M_EditorGetItemCount(void *const user_data)
{
    return UI_SettingsEditor_GetItemCount(user_data);
}

static const UI_SETTINGS_TAB_OPS m_EditorOps = {
    .control = M_EditorControl,
    .draw = M_EditorDraw,
    .draw_footer = M_EditorDrawFooter,
    .draw_overlay = M_EditorDrawOverlay,
    .free = M_EditorFree,
    .get_scrollable = M_EditorGetScrollable,
    .recompute = M_EditorRecompute,
    .get_content_width = M_EditorGetContentWidth,
    .get_content_height = nullptr,
    .get_item_count = M_EditorGetItemCount,
};

UI_SETTINGS_TAB UI_SettingsTab_MakeEditor(
    const GAME_STRING_ID header_gs, const UI_SETTINGS_OPTION *const options)
{
    return (UI_SETTINGS_TAB) {
        .header_gs = header_gs,
        .ops = &m_EditorOps,
        .user_data = UI_SettingsEditor_Init(options),
    };
}

static bool M_PresetsControl(
    void *const user_data, UI_SETTINGS_PHASE *const phase)
{
    if (UI_ConfigPresets_Control(user_data)) {
        *phase = UI_SETTINGS_PHASE_NAVIGATE_TABS;
    }
    return false;
}

static void M_PresetsDraw(
    void *const user_data, const UI_SETTINGS_PHASE, const float)
{
    UI_ConfigPresets(user_data);
}

static void M_PresetsDrawOverlay(void *const user_data)
{
    UI_ConfigPresetsApplyModal(user_data);
}

static void M_PresetsFree(void *const user_data)
{
    UI_ConfigPresets_Free(user_data);
}

static UI_SCROLLABLE *M_PresetsGetScrollable(void *const user_data)
{
    return UI_ConfigPresets_GetScrollable(user_data);
}

static void M_PresetsRecompute(
    void *const user_data, const int32_t visible_rows)
{
    UI_ConfigPresets_RecomputeSizes(user_data, visible_rows);
}

static float M_PresetsGetContentWidth(void *const user_data)
{
    return UI_ConfigPresets_GetContentWidth(user_data);
}

static float M_PresetsGetContentHeight(void *const user_data)
{
    return UI_ConfigPresets_GetContentHeight(user_data);
}

static int32_t M_PresetsGetItemCount(void *const user_data)
{
    return UI_ConfigPresets_GetItemCount(user_data);
}

static const UI_SETTINGS_TAB_OPS m_PresetsOps = {
    .control = M_PresetsControl,
    .draw = M_PresetsDraw,
    .draw_overlay = M_PresetsDrawOverlay,
    .free = M_PresetsFree,
    .get_scrollable = M_PresetsGetScrollable,
    .recompute = M_PresetsRecompute,
    .get_content_width = M_PresetsGetContentWidth,
    .get_content_height = M_PresetsGetContentHeight,
    .get_item_count = M_PresetsGetItemCount,
};

UI_SETTINGS_TAB UI_SettingsTab_MakePresets(const GAME_STRING_ID header_gs)
{
    return (UI_SETTINGS_TAB) {
        .header_gs = header_gs,
        .ops = &m_PresetsOps,
        .user_data = UI_ConfigPresets_Init(),
    };
}
