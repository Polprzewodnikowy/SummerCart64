#include <trx/game/ui/dialogs/config_presets.h>

#include <trx/config.h>
#include <trx/config/common.h>
#include <trx/config/presets.h>
#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/game/game_strings/entries.h>
#include <trx/game/input.h>
#include <trx/game/ui.h>
#include <trx/game/ui/common.h>
#include <trx/game/ui/elements/anchor.h>
#include <trx/game/ui/elements/label.h>
#include <trx/game/ui/elements/modal.h>
#include <trx/game/ui/elements/pad.h>
#include <trx/game/ui/elements/requester.h>
#include <trx/game/ui/elements/resize.h>
#include <trx/game/ui/elements/scrollable_stack.h>
#include <trx/game/ui/elements/spacer.h>
#include <trx/game/ui/elements/stack.h>
#include <trx/game/ui/elements/window.h>
#include <trx/game/ui/scrollable.h>

#include <string.h>

#define M_VISIBLE_ROWS 8
#define M_CONFIRM_VISIBLE_ROWS 10
#define M_CONFIRM_DIALOG_W 72.0f
#define M_LIST_ROW_SPACING 5.0f

typedef enum {
    M_PHASE_BROWSE,
    M_PHASE_CONFIRM,
    M_PHASE_NO_CHANGES,
    M_PHASE_APPLIED,
} M_PHASE;

struct UI_CONFIG_PRESETS_STATE {
    M_PHASE phase;
    UI_REQUESTER_STATE req;
    UI_SCROLLABLE confirm_scroll;
    int32_t selected_idx;
};

static const char *M_GetPresetKeyLabel(const char *const key)
{
    const CONFIG_OPTION *const opt = Config_GetOptionByPath(key);
    if (opt != nullptr) {
        const char *const label = Config_GetOptionTitle(opt);
        if (label != nullptr) {
            return label;
        }
    }
    return key;
}

static int32_t M_GetChangedSettingCount(const int32_t preset_idx)
{
    const CONFIG_PRESET *const preset = Config_Presets_Get(preset_idx);
    if (preset == nullptr || preset->setting_count == 0) {
        return 0;
    }

    int32_t changed_count = 0;
    for (int32_t i = 0; i < preset->setting_count; i++) {
        const CONFIG_OPTION *const opt =
            Config_GetOptionByPath(preset->keys[i]);
        if (opt == nullptr) {
            continue;
        }
        const char *const current_value =
            Config_GetOptionValueAsString(opt, false);
        if (strcmp(current_value, preset->values[i]) != 0) {
            changed_count++;
        }
    }
    return changed_count;
}

static int32_t M_GetConfirmRowCount(const int32_t preset_idx)
{
    return M_GetChangedSettingCount(preset_idx) * 2;
}

static void M_DrawConfirmRows(UI_CONFIG_PRESETS_STATE *const s)
{
    const CONFIG_PRESET *const preset = Config_Presets_Get(s->selected_idx);
    if (preset == nullptr) {
        return;
    }

    for (int32_t i = 0; i < preset->setting_count; i++) {
        const CONFIG_OPTION *const opt =
            Config_GetOptionByPath(preset->keys[i]);
        if (opt == nullptr) {
            continue;
        }
        const char *const current_value_raw =
            Config_GetOptionValueAsString(opt, false);
        if (strcmp(current_value_raw, preset->values[i]) == 0) {
            continue;
        }
        const char *const current_value =
            Config_GetOptionValueAsString(opt, true);
        char *const target_value =
            Config_NormalizeOptionValueString(opt, preset->values[i], true);
        UI_LabelFmt("%s", M_GetPresetKeyLabel(preset->keys[i]));
        UI_LabelFmt("  %s \\{button right} %s", current_value, target_value);
        Memory_Free(target_value);
    }
}

static void M_Header(void *const user_data)
{
    UI_CONFIG_PRESETS_STATE *const s = user_data;
    if (s->phase == M_PHASE_CONFIRM) {
        UI_Label(GS("general/config_presets/confirm_description"));
        UI_Spacer(0.0f, UI_TEXT_HEIGHT);
    }
}

static void M_Footer(void *const user_data)
{
    UI_CONFIG_PRESETS_STATE *const s = user_data;
    if (s->phase == M_PHASE_CONFIRM) {
        UI_Spacer(0.0f, UI_TEXT_HEIGHT);
        UI_Label(GS("general/config_presets/confirm_restart_note"));
    }
}

UI_CONFIG_PRESETS_STATE *UI_ConfigPresets_Init(void)
{
    UI_CONFIG_PRESETS_STATE *const s =
        Memory_Alloc(sizeof(UI_CONFIG_PRESETS_STATE));
    s->phase = M_PHASE_BROWSE;
    s->selected_idx = -1;
    s->confirm_scroll = (UI_SCROLLABLE) {
        .first_item = 0,
        .sel_item = -1,
        .vis_items = M_CONFIRM_VISIBLE_ROWS,
        .max_items = 0,
    };

    const int32_t count = Config_Presets_GetCount();
    UI_Requester_Init(&s->req, M_VISIBLE_ROWS, count, true);
    UI_Requester_SelectRow(&s->req, -1);
    return s;
}

void UI_ConfigPresets_Free(UI_CONFIG_PRESETS_STATE *const s)
{
    UI_Requester_Free(&s->req);
    Memory_Free(s);
}

int32_t UI_ConfigPresets_GetItemCount(UI_CONFIG_PRESETS_STATE *const s)
{
    return Config_Presets_GetCount();
}

void UI_ConfigPresets_RecomputeSizes(
    UI_CONFIG_PRESETS_STATE *const s, const int32_t visible_rows)
{
    int32_t clamped_rows = visible_rows;
    if (clamped_rows < 0) {
        clamped_rows = 0;
    } else if (clamped_rows > M_VISIBLE_ROWS) {
        clamped_rows = M_VISIBLE_ROWS;
    }
    UI_Requester_SetVisibleRows(&s->req, clamped_rows);
}

float UI_ConfigPresets_GetContentWidth(UI_CONFIG_PRESETS_STATE *const s)
{
    return -1.0f;
}

float UI_ConfigPresets_GetContentHeight(UI_CONFIG_PRESETS_STATE *const s)
{
    if (s == nullptr) {
        return -1.0f;
    }
    int32_t rows = s->req.scroll.vis_items;
    if (rows <= 0) {
        return -1.0f;
    }
    return rows * UI_TEXT_HEIGHT + (rows - 1) * M_LIST_ROW_SPACING;
}

UI_SCROLLABLE *UI_ConfigPresets_GetScrollable(UI_CONFIG_PRESETS_STATE *const s)
{
    return &s->req.scroll;
}

bool UI_ConfigPresets_Control(UI_CONFIG_PRESETS_STATE *const s)
{
    if (s->phase == M_PHASE_APPLIED || s->phase == M_PHASE_NO_CHANGES) {
        if (g_InputDB.menu_confirm || g_InputDB.menu_back) {
            s->phase = M_PHASE_BROWSE;
            g_Input = (INPUT_STATE) {};
            g_InputDB = (INPUT_STATE) {};
        }
        return false;
    }

    if (s->phase == M_PHASE_CONFIRM) {
        UI_ScrollableStack_Control(&s->confirm_scroll, UI_STACK_VERTICAL);
        if (g_InputDB.menu_confirm) {
            Config_Presets_Apply(s->selected_idx);
            s->phase = M_PHASE_APPLIED;
            g_Input = (INPUT_STATE) {};
            g_InputDB = (INPUT_STATE) {};
        } else if (g_InputDB.menu_back) {
            s->phase = M_PHASE_BROWSE;
            g_Input = (INPUT_STATE) {};
            g_InputDB = (INPUT_STATE) {};
        }
        return false;
    }

    const int32_t count = Config_Presets_GetCount();
    UI_Requester_SetMaxRows(&s->req, count);
    if (count <= 0) {
        return g_InputDB.menu_back || g_InputDB.menu_up
            || (g_InputDB.menu_down && g_Config.ui.enable_wraparound);
    }

    if (g_InputDB.menu_up && UI_Requester_GetCurrentRow(&s->req) <= 0) {
        return true;
    }
    if (g_InputDB.menu_down && g_Config.ui.enable_wraparound
        && UI_Requester_GetCurrentRow(&s->req) >= count - 1) {
        return true;
    }

    const int32_t choice = UI_Requester_Control(&s->req);
    if (choice == UI_REQUESTER_CANCEL || g_InputDB.menu_back) {
        return true;
    }
    if (choice >= 0 && choice < count) {
        s->selected_idx = choice;
        const int32_t row_count = M_GetConfirmRowCount(choice);
        if (row_count > 0) {
            s->confirm_scroll.first_item = 0;
            s->confirm_scroll.sel_item = -1;
            s->confirm_scroll.max_items = row_count;
            s->phase = M_PHASE_CONFIRM;
        } else {
            s->phase = M_PHASE_NO_CHANGES;
        }
        g_Input = (INPUT_STATE) {};
        g_InputDB = (INPUT_STATE) {};
    }
    return false;
}

void UI_ConfigPresets(UI_CONFIG_PRESETS_STATE *const s)
{
    const int32_t count = Config_Presets_GetCount();

    UI_BeginResize(-1.0f, -1.0f);
    UI_BeginStackEx((UI_STACK_SETTINGS) {
        .orientation = UI_STACK_VERTICAL,
        .align = { .h = UI_STACK_H_ALIGN_SPAN },
        .spacing = { .v = M_LIST_ROW_SPACING },
    });

    const int32_t first_row = UI_Requester_GetFirstRow(&s->req);
    for (int32_t i = 0; i < s->req.scroll.vis_items; i++) {
        const int32_t row = first_row + i;
        if (row < count) {
            const CONFIG_PRESET *const preset = Config_Presets_Get(row);
            UI_BeginRequesterRow(&s->req, row);
            UI_BeginAnchor(0.5f, 0.5f);
            UI_Label(preset != nullptr ? GameString_Get(preset->name_gs) : "");
            UI_EndAnchor();
            UI_EndRequesterRow(&s->req, row);
        } else {
            UI_Spacer(0.0f, UI_TEXT_HEIGHT);
        }
    }

    if (count <= 0) {
        UI_BeginAnchor(0.5f, 0.5f);
        UI_Label(GS("general/config_presets/empty"));
        UI_EndAnchor();
    }

    UI_EndStack();
    UI_EndResize();
}

void UI_ConfigPresetsApplyModal(UI_CONFIG_PRESETS_STATE *const s)
{
    if (s->phase == M_PHASE_BROWSE) {
        return;
    }

    const CONFIG_PRESET *const preset = Config_Presets_Get(s->selected_idx);
    const char *const preset_name =
        preset != nullptr ? GameString_Get(preset->name_gs) : "";
    const char *const title = String_FormatStatic(
        GS("general/config_presets/title_fmt"), preset_name);

    UI_BeginModal(0.5f, 0.5f);
    UI_BeginPad(6.0f, 6.0f);
    UI_BeginWindow((UI_WINDOW_SETTINGS) {
        .title = title,
        .scrollable =
            s->phase == M_PHASE_CONFIRM ? &s->confirm_scroll : nullptr,
        .title_spacing = -1.0f,
        .header_func = M_Header,
        .footer_func = M_Footer,
        .user_data = s,
        .heavy = true,
        .reserve_scroll_space = true,
    });

    if (s->phase == M_PHASE_APPLIED) {
        UI_BeginPad(6.0f, 6.0f);
        UI_LabelFmt("%s", GS("general/config_presets/applied"));
        UI_EndPad();
    } else if (s->phase == M_PHASE_NO_CHANGES) {
        UI_BeginPad(6.0f, 6.0f);
        UI_LabelFmt("%s", GS("general/config_presets/no_changes"));
        UI_EndPad();
    } else if (s->phase == M_PHASE_CONFIRM) {
        UI_BeginResize(M_CONFIRM_DIALOG_W, -1.0f);

        UI_BeginScrollableStack(
            &s->confirm_scroll,
            (UI_SCROLLABLE_STACK_SETTINGS) {
                .orientation = UI_STACK_VERTICAL,
                .spacing = 2.0f,
            });
        M_DrawConfirmRows(s);
        UI_EndScrollableStack();
        UI_EndResize();
    }

    UI_EndWindow();
    UI_EndPad();
    UI_EndModal();
}
