#include <trx/game/ui/dialogs/settings_editor.h>

#include <trx/config.h>
#include <trx/config/dynamic_enum.h>
#include <trx/core/enum_map.h>
#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/core/utils.h>
#include <trx/debug.h>
#include <trx/game/input.h>
#include <trx/game/ui.h>
#include <trx/game/ui/dialogs/color_editor.h>
#include <trx/game/ui/dialogs/setting_helpers/enums.h>
#include <trx/game/ui/dialogs/text.h>
#include <trx/version.h>

#include <math.h>

#define M_BAR_WIDTH 60
#define M_BAR_HEIGHT 12

typedef struct {
    int32_t position;
    int32_t count;
} M_ENUM_LOOKUP;

typedef struct UI_SETTINGS_EDITOR_STATE {
    const UI_SETTINGS_OPTION *options;
    int32_t visible_rows;
    UI_SCROLLABLE scroll;
    struct {
        bool show;
        UI_TEXT_DIALOG_STATE *state;
    } description;
    UI_COLOR_EDITOR_DIALOG_STATE *color_editor;
} UI_SETTINGS_EDITOR_STATE;

static const CONFIG_OPTION *M_GetConfigOption(
    const UI_SETTINGS_OPTION *const option)
{
    ASSERT(option != nullptr);
    ASSERT(option->target != nullptr);
    const CONFIG_OPTION *const result = Config_GetOption(option->target);
    ASSERT(result != nullptr);
    return result;
}

static const char *M_GetOptionDescription(
    const UI_SETTINGS_OPTION *const option)
{
    if (option == nullptr || option->target == nullptr) {
        return nullptr;
    }
    return Config_GetOptionDescription(M_GetConfigOption(option));
}

static const char *M_GetOptionTitle(const UI_SETTINGS_OPTION *const option)
{
    if (option == nullptr || option->target == nullptr) {
        return "";
    }
    const char *const result = Config_GetOptionTitle(M_GetConfigOption(option));
    return result != nullptr ? result : "";
}

static bool M_IsEnumEntryAvailable(
    const UI_SETTINGS_OPTION *const option,
    const UI_SETTINGS_ENUM_ENTRY *const entry)
{
    if (entry == nullptr || entry->value == -1) {
        return false;
    }
    if (option->custom_handler.is_enum_value_available == nullptr) {
        return true;
    }
    return option->custom_handler.is_enum_value_available(option, entry->value);
}

static UI_BAR_TYPE M_GetBarType(const UI_SETTINGS_OPTION *const option)
{
    if (option->target == &g_Config.ui.lara_health_bar.color
        || option->target == &g_Config.ui.lara_health_bar.color_ps1) {
        return UI_BAR_LARA_HP;
    } else if (
        option->target == &g_Config.ui.lara_health_bar.poison_color
        || option->target == &g_Config.ui.lara_health_bar.poison_color_ps1) {
        return UI_BAR_LARA_HP_POISON;
    } else if (
        option->target == &g_Config.ui.lara_air_bar.color
        || option->target == &g_Config.ui.lara_air_bar.color_ps1) {
        return UI_BAR_LARA_AIR;
    } else if (
        option->target == &g_Config.ui.lara_sprint_bar.color
        || option->target == &g_Config.ui.lara_sprint_bar.color_ps1) {
        return UI_BAR_LARA_STAMINA;
    } else if (
        option->target == &g_Config.ui.lara_exposure_bar.color
        || option->target == &g_Config.ui.lara_exposure_bar.color_ps1) {
        return UI_BAR_LARA_EXPOSURE;
    } else if (
        option->target == &g_Config.ui.enemy_health_bar.color
        || option->target == &g_Config.ui.enemy_health_bar.color_ps1) {
        return UI_BAR_ENEMY_HP;
    } else if (
        option->target == &g_Config.ui.enemy_health_bar.color_allies
        || option->target == &g_Config.ui.enemy_health_bar.color_allies_ps1) {
        return UI_BAR_ALLY_HP;
    } else {
        return (UI_BAR_TYPE)-1;
    }
}

static bool M_IsBarColorEnum(const UI_SETTINGS_OPTION *const option)
{
    return M_GetBarType(option) != (UI_BAR_TYPE)-1;
}

static bool M_IsColorEditorOption(const UI_SETTINGS_OPTION *const option)
{
    return option != nullptr && M_GetConfigOption(option)->type == COT_RGB888;
}

static bool M_HasAvailableEnumValue(const UI_SETTINGS_OPTION *const option)
{
    const UI_SETTINGS_ENUM_ENTRY *entry =
        (UI_SETTINGS_ENUM_ENTRY *)option->misc;
    if (entry == nullptr) {
        return false;
    }
    while (entry->value != -1) {
        if (M_IsEnumEntryAvailable(option, entry)) {
            return true;
        }
        entry++;
    }
    return false;
}

static bool M_IsOptionHidden(const UI_SETTINGS_OPTION *const option)
{
    if (option->custom_handler.is_visible != nullptr
        && !option->custom_handler.is_visible(option)) {
        return true;
    }
    if (Config_IsOptionHidden(option->target)) {
        return true;
    }
    if (M_GetConfigOption(option)->type == COT_ENUM && option->misc != nullptr
        && !M_HasAvailableEnumValue(option)) {
        return true;
    }
    return false;
}

static const UI_SETTINGS_OPTION *M_GetOptionByRow(
    const UI_SETTINGS_EDITOR_STATE *const s, const int32_t row_idx)
{
    if (s->options == nullptr) {
        return nullptr;
    }
    int32_t count = 0;
    for (int32_t i = 0; s->options[i].target != nullptr; i++) {
        const UI_SETTINGS_OPTION *const opt = &s->options[i];
        if (M_IsOptionHidden(opt)) {
            continue;
        }
        if (count == row_idx) {
            return opt;
        }
        count++;
    }
    return nullptr;
}

static int32_t M_GetRowCount(const UI_SETTINGS_EDITOR_STATE *const s)
{
    if (s->options == nullptr) {
        return 0;
    }
    int32_t count = 0;
    for (int32_t i = 0; s->options[i].target != nullptr; i++) {
        if (!M_IsOptionHidden(&s->options[i])) {
            count++;
        }
    }
    return count;
}

static M_ENUM_LOOKUP M_GetEnumEntry(const UI_SETTINGS_OPTION *const option)
{
    M_ENUM_LOOKUP result = {
        .position = -1,
        .count = 0,
    };
    int32_t current_pos = 0;
    const UI_SETTINGS_ENUM_ENTRY *entry =
        &((UI_SETTINGS_ENUM_ENTRY *)option->misc)[0];
    while (entry->value != -1) {
        if (entry->value == *(int32_t *)option->target) {
            result.position = current_pos;
        }
        entry++;
        current_pos++;
        result.count++;
    }
    return result;
}

static int32_t M_FindNextAvailableEnumPosition(
    const UI_SETTINGS_OPTION *const option,
    const M_ENUM_LOOKUP *const enum_lookup, const int32_t dir)
{
    if (enum_lookup->position < 0 || enum_lookup->count <= 0 || dir == 0) {
        return -1;
    }
    const UI_SETTINGS_ENUM_ENTRY *const entries = option->misc;
    const int32_t step = dir < 0 ? -1 : 1;

    for (int32_t pos = enum_lookup->position + step;
         pos >= 0 && pos < enum_lookup->count; pos += step) {
        if (M_IsEnumEntryAvailable(option, &entries[pos])) {
            return pos;
        }
    }

    return -1;
}

static const char *M_FormatRowValue(
    const UI_SETTINGS_EDITOR_STATE *const s, const int32_t row_idx)
{
    const UI_SETTINGS_OPTION *const option = M_GetOptionByRow(s, row_idx);
    if (option == nullptr) {
        return nullptr;
    }
    if (option->custom_handler.format_value != nullptr) {
        return option->custom_handler.format_value(option);
    }

    const CONFIG_OPTION *const cfg_opt = M_GetConfigOption(option);
    return Config_GetOptionValueAsString(cfg_opt, true);
}

static float M_MeasureMaxValueWidth(const UI_SETTINGS_OPTION *const option)
{
    if (option->custom_handler.format_value != nullptr) {
        const char *const value = option->custom_handler.format_value(option);
        const float result = UI_Label_MeasureW(value);
        return result;
    }

    if (M_IsBarColorEnum(option)) {
        return M_BAR_WIDTH * g_Config.ui.text_scale;
    }

    switch (M_GetConfigOption(option)->type) {
    case COT_BOOL: {
        const float min_value_w = UI_Label_MeasureW(GS("general/misc/off"));
        const float max_value_w = UI_Label_MeasureW(GS("general/misc/on"));
        return MAX(min_value_w, max_value_w);
    }
    case COT_INT32: {
        const char *const min_value_s =
            String_FormatStatic("%d", option->min_value);
        const float min_value_w = UI_Label_MeasureW(min_value_s);
        const char *const max_value_s =
            String_FormatStatic("%d", option->max_value);
        const float max_value_w = UI_Label_MeasureW(max_value_s);
        return MAX(min_value_w, max_value_w);
    }
    case COT_DOUBLE:
    case COT_FLOAT: {
        const char *const min_value_s =
            String_FormatStatic("%.2f", (double)option->min_value / 100.0);
        const float min_value_w = UI_Label_MeasureW(min_value_s);
        const char *const max_value_s =
            String_FormatStatic("%.2f", (double)option->max_value / 100.0);
        const float max_value_w = UI_Label_MeasureW(max_value_s);
        return MAX(min_value_w, max_value_w);
    }
    case COT_FLOAT_PERCENT: {
        const char *const min_value_s =
            String_FormatStatic("%.00f%%", (double)option->min_value);
        const float min_value_w = UI_Label_MeasureW(min_value_s);
        const char *const max_value_s =
            String_FormatStatic("%.00f%%", (double)option->max_value);
        const float max_value_w = UI_Label_MeasureW(max_value_s);
        return MAX(min_value_w, max_value_w);
    }
    case COT_RGB888:
        return UI_Label_MeasureW("#FFFFFF") + 8.0f * g_Config.ui.text_scale
            + 32.0f * g_Config.ui.text_scale;
    case COT_STRING:
        return UI_Label_MeasureW(*(char **)option->target);
    case COT_DYNAMIC_ENUM: {
        const CONFIG_OPTION *const cfg_opt = M_GetConfigOption(option);
        float result = 0.0f;
        const int32_t count = Config_DynamicEnum_GetValueCount(cfg_opt);
        for (int32_t i = 0; i < count; i++) {
            const char *const label = Config_DynamicEnum_GetLabelAt(cfg_opt, i);
            result = MAX(result, UI_Label_MeasureW(label));
        }
        return result;
    }
    case COT_ENUM: {
        const CONFIG_OPTION *const cfg_opt = M_GetConfigOption(option);
        float result = 0.0f;
        const UI_SETTINGS_ENUM_ENTRY *entry = option->misc;
        const int32_t current_value = *(int32_t *)option->target;
        while (entry->value != -1) {
            const bool is_current = entry->value == current_value;
            if (!is_current && !M_IsEnumEntryAvailable(option, entry)) {
                entry++;
                continue;
            }
            const char *const value =
                EnumMap_GetLabel(cfg_opt->param, entry->value);
            ASSERT(value != nullptr);
            const float value_w = UI_Label_MeasureW(value);
            result = MAX(result, value_w);
            entry++;
        }
        return result;
    }
    default:
        break;
    }
    return 0.0f;
}

static bool M_CanChangeValue(
    const UI_SETTINGS_EDITOR_STATE *const s, const int32_t row_idx,
    const int32_t dir)
{
    const UI_SETTINGS_OPTION *const option = M_GetOptionByRow(s, row_idx);
    if (option == nullptr || Config_IsOptionEnforced(option->target)) {
        return false;
    }
    if (option->custom_handler.can_change_value != nullptr) {
        return option->custom_handler.can_change_value(option, dir);
    }

    switch (M_GetConfigOption(option)->type) {
    case COT_BOOL:
        return true;

    case COT_INT32:
        if (dir < 0) {
            return *(int32_t *)option->target > option->min_value;
        } else if (dir > 0) {
            return *(int32_t *)option->target < option->max_value;
        }
        break;

    case COT_DOUBLE: {
        const double target_value =
            (round(*(double *)option->target * 100) + dir) / 100.0;
        return target_value >= (double)option->min_value / 100.0
            && target_value <= (double)option->max_value / 100.0;
    }

    case COT_FLOAT:
    case COT_FLOAT_PERCENT: {
        const float target_value =
            (round(*(float *)option->target * 100) + dir) / 100.0f;
        return target_value >= (float)option->min_value / 100.0f
            && target_value <= (float)option->max_value / 100.0f;
    }

    case COT_RGB888:
        return false;

    case COT_STRING:
        return false;

    case COT_DYNAMIC_ENUM: {
        const CONFIG_OPTION *const cfg_opt = M_GetConfigOption(option);
        return Config_DynamicEnum_CanCycle(
            cfg_opt, *(char **)option->target, dir);
    }

    case COT_ENUM: {
        const M_ENUM_LOOKUP enum_lookup = M_GetEnumEntry(option);
        ASSERT(enum_lookup.position >= 0);
        return M_FindNextAvailableEnumPosition(option, &enum_lookup, dir) >= 0;
    }

    default:
        break;
    }
    return false;
}

static bool M_RequestChangeValue(
    const UI_SETTINGS_EDITOR_STATE *const s, const int32_t row_idx,
    const int32_t dir)
{
    if (!M_CanChangeValue(s, row_idx, dir)) {
        return false;
    }

    const UI_SETTINGS_OPTION *const option = M_GetOptionByRow(s, row_idx);
    if (option->custom_handler.request_change_value != nullptr) {
        if (option->custom_handler.request_change_value(option, dir)) {
            goto changed;
        }
        return false;
    }

    UI_SettingsEditor_RequestChange(option, dir);
changed:
    Config_Update();
    return true;
}

UI_SETTINGS_EDITOR_STATE *UI_SettingsEditor_Init(
    const UI_SETTINGS_OPTION *const options)
{
    UI_SETTINGS_EDITOR_STATE *const s = Memory_Alloc(sizeof(*s));
    s->options = options;
    s->scroll = (UI_SCROLLABLE) {
        .first_item = 0,
        .sel_item = -1,
        .max_items = 0,
        .vis_items = 0,
    };
    s->color_editor = UI_ColorEditorDialog_Init();
    return s;
}

void UI_SettingsEditor_Free(UI_SETTINGS_EDITOR_STATE *const s)
{
    if (s->description.show) {
        UI_TextDialog_Free(s->description.state);
        s->description.state = nullptr;
        s->description.show = false;
    }
    UI_ColorEditorDialog_Free(s->color_editor);
    s->color_editor = nullptr;
    Memory_Free(s);
}

static float M_GetMaxLabelWidth(const UI_SETTINGS_EDITOR_STATE *const s)
{
    float result = -1.0f;
    if (s->options != nullptr) {
        for (int32_t i = 0; s->options[i].target != nullptr; i++) {
            const UI_SETTINGS_OPTION *const option = &s->options[i];
            const float label_w = UI_Label_MeasureW(M_GetOptionTitle(option));
            result = MAX(label_w, result);
        }
    }
    return result;
}

static float M_GetMaxValueWidth(const UI_SETTINGS_EDITOR_STATE *const s)
{
    float result = -1.0f;
    if (s->options != nullptr) {
        for (int32_t i = 0; s->options[i].target != nullptr; i++) {
            const UI_SETTINGS_OPTION *const option = &s->options[i];
            const float value_w = M_MeasureMaxValueWidth(option);
            result = MAX(value_w, result);
        }
    }

    result += UI_Label_MeasureW("\\{button left}");
    result += UI_Label_MeasureW("\\{button right}");
    result += UI_ROW_ARROWS_TIGHT * 2;
    return result;
}

float UI_SettingsEditor_GetContentWidth(const UI_SETTINGS_EDITOR_STATE *const s)
{
    return M_GetMaxLabelWidth(s) + 20.0f * g_Config.ui.text_scale
        + M_GetMaxValueWidth(s);
}

int32_t UI_SettingsEditor_GetItemCount(const UI_SETTINGS_EDITOR_STATE *const s)
{
    return M_GetRowCount(s);
}

void UI_SettingsEditor_RequestChange(
    const UI_SETTINGS_OPTION *const option, const int32_t dir)
{
    int32_t delta = g_Input.slow ? option->delta_slow : option->delta_fast;
    if (delta == 0) {
        delta = 1;
    }
    delta *= dir;

    switch (M_GetConfigOption(option)->type) {
    case COT_BOOL:
        *(bool *)option->target = !*(bool *)option->target;
        break;
    case COT_INT32:
        *(int32_t *)option->target += delta;
        break;
    case COT_DOUBLE:
        *(double *)option->target =
            (round(*(double *)option->target * 100) + delta) / 100.0f;
        if (*(double *)option->target == -0.0) {
            *(double *)option->target = 0.0;
        }
        break;
    case COT_FLOAT:
    case COT_FLOAT_PERCENT:
        *(float *)option->target =
            (round(*(float *)option->target * 100) + delta) / 100.0f;
        if (*(float *)option->target == -0.0f) {
            *(float *)option->target = 0.0f;
        }
        break;
    case COT_RGB888:
        break;
    case COT_ENUM: {
        const UI_SETTINGS_ENUM_ENTRY *const entries = option->misc;
        int32_t position = -1;
        int32_t count = 0;
        for (; entries[count].value != -1; count++) {
            if (entries[count].value == *(int32_t *)option->target) {
                position = count;
            }
        }
        if (position < 0 || count <= 0 || delta == 0) {
            break;
        }
        const int32_t step = delta < 0 ? -1 : 1;
        for (int32_t pos = position + step; pos >= 0 && pos < count;
             pos += step) {
            const bool can_use =
                option->custom_handler.is_enum_value_available == nullptr
                || option->custom_handler.is_enum_value_available(
                    option, entries[pos].value);
            if (can_use) {
                *(int32_t *)option->target = entries[pos].value;
                break;
            }
        }
        break;
    }
    case COT_DYNAMIC_ENUM: {
        const CONFIG_OPTION *const cfg_opt = M_GetConfigOption(option);
        const char *const next = Config_DynamicEnum_GetNext(
            cfg_opt, *(char **)option->target, delta);
        if (next != nullptr
            || Config_DynamicEnum_IsValidValue(cfg_opt, nullptr)) {
            Config_SetOptionValueFromString(cfg_opt, next);
        }
        break;
    }
    case COT_STRING:
        break;
    }
}

void UI_SettingsEditor_RecomputeSizes(
    UI_SETTINGS_EDITOR_STATE *const s, const int32_t visible_rows)
{
    s->visible_rows = visible_rows;
    UI_Scrollable_SetMaxItems(&s->scroll, M_GetRowCount(s));
    UI_Scrollable_SetVisibleItems(&s->scroll, visible_rows);
}

UI_SCROLLABLE *UI_SettingsEditor_GetScrollable(
    UI_SETTINGS_EDITOR_STATE *const s)
{
    return &s->scroll;
}

bool UI_SettingsEditor_Control(
    UI_SETTINGS_EDITOR_STATE *const s, UI_SETTINGS_PHASE *const dialog_phase)
{
    if (UI_ColorEditorDialog_IsOpen(s->color_editor)) {
        UI_ColorEditorDialog_Control(s->color_editor);
        return true;
    }
    if (s->description.show) {
        UI_TextDialog_Control(s->description.state);
        if (g_InputDB.menu_back || g_InputDB.look) {
            UI_TextDialog_Free(s->description.state);
            s->description.state = nullptr;
            s->description.show = false;
        }
        return true;
    }

    const int32_t sel_row = UI_Scrollable_GetSelectedItem(&s->scroll);

    if (g_InputDB.menu_left && sel_row >= 0) {
        M_RequestChangeValue(s, sel_row, -1);
        return true;
    }
    if (g_InputDB.menu_right && sel_row >= 0) {
        M_RequestChangeValue(s, sel_row, +1);
        return true;
    }

    if (g_InputDB.menu_up) {
        if (!UI_Scrollable_SelectPrev(&s->scroll, false)) {
            *dialog_phase = UI_SETTINGS_PHASE_NAVIGATE_TABS;
        }
        return true;
    }
    if (g_InputDB.menu_down) {
        if (!UI_Scrollable_SelectNext(&s->scroll, false)
            && g_Config.ui.enable_wraparound) {
            *dialog_phase = UI_SETTINGS_PHASE_NAVIGATE_TABS;
        }
        return true;
    }
    if (g_InputDB.menu_confirm && sel_row >= 0) {
        const UI_SETTINGS_OPTION *const option = M_GetOptionByRow(s, sel_row);
        if (M_IsColorEditorOption(option)) {
            UI_ColorEditorDialog_Open(s->color_editor, option);
            return true;
        }
    }
    if (g_InputDB.look && sel_row >= 0) {
        const UI_SETTINGS_OPTION *const option = M_GetOptionByRow(s, sel_row);
        const char *const title = M_GetOptionTitle(option);
        const char *const text = M_GetOptionDescription(option);
        if (title != nullptr && text != nullptr) {
            s->description.show = true;
            s->description.state = UI_TextDialog_Init(
                UI_GetCanvasWidth() * 2.0f / 3.0f, (size_t)s->visible_rows,
                true);
            return true;
        }
    }
    if (g_InputDB.unbind_key && sel_row >= 0) {
        const UI_SETTINGS_OPTION *const option = M_GetOptionByRow(s, sel_row);
        if (option != nullptr && option->target != nullptr
            && !Config_IsOptionEnforced(option->target)
            && !Config_IsOptionAtDefault(option->target)) {
            Config_RestoreOptionDefault(option->target);
            Config_Update();
            return true;
        }
    }

    return false;
}

void UI_SettingsEditor_DrawOverlay(UI_SETTINGS_EDITOR_STATE *const s)
{
    UI_ColorEditorDialog(s->color_editor);

    if (s->description.show) {
        const int32_t row = UI_Scrollable_GetSelectedItem(&s->scroll);
        const UI_SETTINGS_OPTION *const option = M_GetOptionByRow(s, row);
        if (option != nullptr) {
            const char *title = M_GetOptionTitle(option);
            const char *text = M_GetOptionDescription(option);
            if (title != nullptr && text != nullptr) {
                if (Config_IsOptionEnforced(option->target)) {
                    title = String_FormatStatic("%s*", title);
                    text = String_FormatStatic(
                        "* %s\n\n%s",
                        *GS_PTR(
                            "general/settings/common/frozen_option_disclaimer"),
                        text);
                }
                UI_TextDialog(s->description.state, title, text);
            }
        }
    }
}

static void M_OptionLabel(
    const UI_SETTINGS_OPTION *const option, const char *const text,
    const bool star_if_enforced)
{
    const bool is_available = option == nullptr
        || option->custom_handler.is_available == nullptr
        || option->custom_handler.is_available(option);
    const bool is_enforced = star_if_enforced && option != nullptr
        && Config_IsOptionEnforced(option->target);
    const char *const suffix = is_enforced ? "*" : "";

    if (!is_available) {
        UI_LabelFmt("\\{dim}%s%s\\{/dim}", text, suffix);
    } else if (is_enforced) {
        UI_LabelFmt("%s%s", text, suffix);
    } else {
        UI_Label(text);
    }
}

void UI_SettingsEditor_Draw(
    UI_SETTINGS_EDITOR_STATE *const s, const UI_SCROLLABLE *const dialog_scroll,
    const UI_SETTINGS_PHASE dialog_phase, const float row_width)
{
    const float max_label_w = M_GetMaxLabelWidth(s) / g_Config.ui.text_scale;
    const float max_value_w = M_GetMaxValueWidth(s) / g_Config.ui.text_scale;
    float label_w = max_label_w;
    const float total_w = max_label_w + 20.0f + max_value_w;
    if (row_width > total_w) {
        label_w += row_width - total_w;
    }

    const int32_t sel_row = UI_Scrollable_GetSelectedItem(dialog_scroll);

    if (dialog_scroll->vis_items == 0) {
        return;
    }

    UI_BeginStack(UI_STACK_VERTICAL);
    for (int32_t i = 0; i < dialog_scroll->vis_items; i++) {
        const int32_t row = dialog_scroll->first_item + i;
        if (row >= dialog_scroll->max_items) {
            UI_Spacer(0.0f, UI_TEXT_HEIGHT);
            continue;
        }

        const bool is_row_focused =
            dialog_phase == UI_SETTINGS_PHASE_EDIT_SETTINGS && row == sel_row;
        if (!UI_Scrollable_IsItemVisible(dialog_scroll, row)) {
            UI_BeginResize(-1.0f, 0.0f);
        } else {
            UI_BeginResize(-1.0f, -1.0f);
        }

        UI_BeginPad(
            g_TRVersion == 1 ? -1.0f : 0.0f, g_TRVersion == 1 ? -1.0f : 0.0f);
        if (is_row_focused) {
            UI_BeginFrame(UI_FRAME_SELECTED_OPTION);
        }
        UI_BeginPad(
            (g_TRVersion == 1 ? 1.0f : 0.0f), g_TRVersion == 1 ? 1.0f : 0.0f);
        UI_BeginStackEx((UI_STACK_SETTINGS) {
            .orientation = UI_STACK_HORIZONTAL,
            .align = { .h = UI_STACK_H_ALIGN_DISTRIBUTE },
        });
        UI_BeginResize(label_w, -1.0f);
        {
            const UI_SETTINGS_OPTION *const option = M_GetOptionByRow(s, row);
            const char *const name =
                option != nullptr ? M_GetOptionTitle(option) : "";
            M_OptionLabel(option, name, true);
        }
        UI_EndResize();
        UI_Spacer(20.0f, 0.0f);

        UI_BeginResize(max_value_w, -1.0f);
        UI_BeginAnchor(1.0f, 0.5f);

        UI_BeginRowArrows(
            is_row_focused && M_CanChangeValue(s, row, -1),
            is_row_focused && M_CanChangeValue(s, row, +1),
            UI_ROW_ARROWS_MEDIUM);
        {
            const UI_SETTINGS_OPTION *const option = M_GetOptionByRow(s, row);
            if (M_IsBarColorEnum(option)) {
                UI_Bar((UI_BAR_SETTINGS) {
                    .w = M_BAR_WIDTH,
                    .h = M_BAR_HEIGHT,
                    .value = 100,
                    .max_value = 100,
                    .type = M_GetBarType(option),
                    .preview = true,
                });
            } else if (M_IsColorEditorOption(option)) {
                const char *const value = M_FormatRowValue(s, row);
                const RGB_888 *const color = option->target;
                UI_BeginStackEx((UI_STACK_SETTINGS) {
                    .orientation = UI_STACK_HORIZONTAL,
                    .align = { .v = UI_STACK_V_ALIGN_CENTER },
                    .spacing = { .h = 4.0f },
                });
                M_OptionLabel(option, value, false);
                UI_ColorSwatch((UI_COLOR_SWATCH_SETTINGS) {
                    .color = *color,
                    .w = UI_TEXT_HEIGHT - 2.0f,
                    .h = UI_TEXT_HEIGHT - 2.0f,
                });
                UI_EndStack();
            } else {
                const char *const value = M_FormatRowValue(s, row);
                M_OptionLabel(option, value, false);
            }
        }
        UI_EndRowArrows();

        UI_EndAnchor();
        UI_EndResize();

        UI_EndStack();

        UI_EndPad();
        if (is_row_focused) {
            UI_EndFrame();
        }
        UI_EndPad();

        UI_EndResize();
    }
    UI_EndStack();
}

void UI_SettingsEditor_DrawFooter(
    UI_SETTINGS_EDITOR_STATE *const s, const UI_SETTINGS_PHASE dialog_phase)
{
    const int32_t row_idx = UI_Scrollable_GetSelectedItem(&s->scroll);
    const UI_SETTINGS_OPTION *const option = M_GetOptionByRow(s, row_idx);

    const bool can_edit_value = dialog_phase == UI_SETTINGS_PHASE_EDIT_SETTINGS
        && row_idx >= 0 && option != nullptr
        && M_GetConfigOption(option)->type == COT_RGB888;
    const bool can_examine = dialog_phase == UI_SETTINGS_PHASE_EDIT_SETTINGS
        && row_idx >= 0 && option != nullptr
        && M_GetOptionDescription(option) != nullptr
        && M_GetOptionTitle(option) != nullptr;
    const bool can_restore_default =
        dialog_phase == UI_SETTINGS_PHASE_EDIT_SETTINGS && row_idx >= 0
        && option != nullptr && option->target != nullptr
        && !Config_IsOptionEnforced(option->target)
        && !Config_IsOptionAtDefault(option->target);

    UI_BeginStackEx((UI_STACK_SETTINGS) {
        .orientation = UI_STACK_HORIZONTAL,
        .align = { .h = UI_STACK_H_ALIGN_DISTRIBUTE },
        .spacing = { .h = 20 },
    });
    UI_BeginHide(!can_examine && !can_edit_value);
    if (can_edit_value) {
        UI_LabelFmt(
            "\\{input action} %s", GS("general/settings/common/edit_value"));
    } else {
        UI_LabelFmt(
            "\\{input look} %s", GS("general/settings/common/toggle_help"));
    }
    UI_EndHide();
    UI_BeginHide(!can_restore_default);
    UI_LabelFmt(
        "\\{input unbind_key} %s",
        GS("general/settings/common/restore_default"));
    UI_EndHide();
    UI_EndStack();
}
