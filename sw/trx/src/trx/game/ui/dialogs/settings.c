#include <trx/game/ui/dialogs/settings.h>

#include <trx/config.h>
#include <trx/core/memory.h>
#include <trx/core/utils.h>
#include <trx/debug.h>
#include <trx/game/game_strings/manager.h>
#include <trx/game/input.h>
#include <trx/game/ui.h>
#include <trx/game/ui/scaler.h>
#include <trx/game/viewport.h>

typedef struct UI_SETTINGS_DIALOG_STATE {
    UI_SETTINGS_PHASE phase;
    int32_t visible_rows;

    float max_content_width;
    float max_content_height;

    int32_t tab_count;
    UI_SETTINGS_TAB *tabs;
    UI_TAB_SWITCH_STATE *tab_switch;
    GAME_STRING_ID title;

    int32_t listener_id;
} UI_SETTINGS_DIALOG_STATE;

static int32_t M_GetVisibleRows(void)
{
    const int32_t res_h = UI_Scaler_CalcInverse(
        Viewport_GetHeight(VIEWPORT_UI), UI_SCALER_TARGET_TEXT);
    static struct {
        int32_t threshold;
        int32_t rows;
    } thresholds[] = {
        { 240, 5 },  { 252, 6 },  { 266, 7 },  { 282, 8 },
        { 300, 9 },  { 320, 10 }, { 342, 11 }, { 370, 12 },
        { 420, 13 }, { 480, 15 }, { -1, 16 },
    };
    for (int32_t i = 0;; i++) {
        if (res_h <= thresholds[i].threshold || thresholds[i].threshold == -1) {
            return thresholds[i].rows;
        }
    }
}

static UI_SETTINGS_TAB *M_GetActiveTab(UI_SETTINGS_DIALOG_STATE *const s)
{
    if (s->tab_switch == nullptr || s->tabs == nullptr || s->tab_count <= 0) {
        return nullptr;
    }
    const int32_t idx = s->tab_switch->active_tab_idx;
    if (idx < 0 || idx >= s->tab_count) {
        return nullptr;
    }
    return &s->tabs[idx];
}

static const UI_SETTINGS_TAB *M_GetActiveTabConst(
    const UI_SETTINGS_DIALOG_STATE *const s)
{
    if (s->tab_switch == nullptr || s->tabs == nullptr || s->tab_count <= 0) {
        return nullptr;
    }
    const int32_t idx = s->tab_switch->active_tab_idx;
    if (idx < 0 || idx >= s->tab_count) {
        return nullptr;
    }
    return &s->tabs[idx];
}

static UI_SCROLLABLE *M_GetTabScrollable(UI_SETTINGS_TAB *const tab)
{
    if (tab == nullptr || tab->ops == nullptr
        || tab->ops->get_scrollable == nullptr) {
        return nullptr;
    }
    return tab->ops->get_scrollable(tab->user_data);
}

static void M_RecomputeSizes(UI_SETTINGS_DIALOG_STATE *const s)
{
    int32_t max_item_count = 0;
    for (int32_t i = 0; i < s->tab_count; i++) {
        UI_SETTINGS_TAB *const tab = &s->tabs[i];
        if (tab->ops != nullptr && tab->ops->get_item_count != nullptr) {
            max_item_count =
                MAX(max_item_count, tab->ops->get_item_count(tab->user_data));
        }
    }

    const int32_t visible_rows = MIN(max_item_count, M_GetVisibleRows());
    float max_content_width = 0.0f;
    float max_content_height = -1.0f;

    for (int32_t i = 0; i < s->tab_count; i++) {
        UI_SETTINGS_TAB *const tab = &s->tabs[i];
        if (tab->ops != nullptr && tab->ops->recompute != nullptr) {
            tab->ops->recompute(tab->user_data, visible_rows);
        }
        if (tab->ops != nullptr && tab->ops->get_content_width != nullptr) {
            max_content_width = MAX(
                max_content_width, tab->ops->get_content_width(tab->user_data));
        }
        if (tab->ops != nullptr) {
            const UI_SCROLLABLE *const tab_scroll = M_GetTabScrollable(tab);
            const int32_t tab_visible_rows =
                tab_scroll != nullptr ? tab_scroll->vis_items : visible_rows;
            float tab_content_height = -1.0f;
            if (tab->ops->get_content_height != nullptr) {
                tab_content_height =
                    tab->ops->get_content_height(tab->user_data);
            } else if (tab_visible_rows > 0) {
                tab_content_height = tab_visible_rows * UI_TEXT_HEIGHT;
            }
            max_content_height = MAX(max_content_height, tab_content_height);
        }
    }

    s->visible_rows = visible_rows;
    s->max_content_width = max_content_width / g_Config.ui.text_scale;
    s->max_content_height = max_content_height;
}

static void M_WindowHeader(void *const user_data)
{
    UI_SETTINGS_DIALOG_STATE *const s = user_data;
    if (s->tab_switch != nullptr && s->tab_count > 0) {
        UI_TabSwitch(
            s->tab_switch, s->phase == UI_SETTINGS_PHASE_NAVIGATE_TABS);
        UI_Spacer(0.0f, 8.0f);
    }
}

static void M_HandleLanguageReload(const EVENT *const, void *const data)
{
    UI_SETTINGS_DIALOG_STATE *const s = data;
    M_RecomputeSizes(s);
}

static UI_SETTINGS_DIALOG_STATE *M_InitCommon(const GAME_STRING_ID title)
{
    UI_SETTINGS_DIALOG_STATE *const s = Memory_Alloc(sizeof(*s));
    s->title = title;
    s->listener_id =
        GameStringManager_SubscribeReload(M_HandleLanguageReload, s);
    return s;
}

static void M_SetActiveTab(UI_SETTINGS_DIALOG_STATE *const s, const int32_t idx)
{
    s->tab_switch->active_tab_idx = idx;
    M_RecomputeSizes(s);
}

static void M_EnterEditMode(
    UI_SETTINGS_DIALOG_STATE *const s, const bool focus_last)
{
    s->phase = UI_SETTINGS_PHASE_EDIT_SETTINGS;
    UI_SETTINGS_TAB *const tab = M_GetActiveTab(s);
    UI_SCROLLABLE *const active_scroll = M_GetTabScrollable(tab);
    if (active_scroll != nullptr) {
        if (focus_last) {
            UI_Scrollable_SelectLastItem(active_scroll);
        } else {
            UI_Scrollable_SelectFirstItem(active_scroll);
        }
    }
}

static void M_ClearActiveCustomSelection(UI_SETTINGS_DIALOG_STATE *const s)
{
    UI_SETTINGS_TAB *const tab = M_GetActiveTab(s);
    if (tab == nullptr || tab->ops == nullptr
        || tab->ops->get_content_height == nullptr) {
        return;
    }
    UI_SCROLLABLE *const active_scroll = M_GetTabScrollable(tab);
    if (active_scroll != nullptr) {
        UI_Scrollable_SelectItem(active_scroll, -1);
    }
}

UI_SETTINGS_DIALOG_STATE *UI_SettingsDialog_Init(
    const GAME_STRING_ID title, const int32_t tab_count,
    const UI_SETTINGS_TAB *const tabs)
{
    ASSERT(tabs != nullptr);
    UI_SETTINGS_DIALOG_STATE *const s = M_InitCommon(title);

    int32_t visible_tab_count = 0;
    for (int32_t i = 0; i < tab_count; i++) {
        const UI_SETTINGS_TAB *const tab = &tabs[i];
        int32_t item_count = 0;
        if (tab->ops != nullptr && tab->ops->get_item_count != nullptr) {
            item_count = tab->ops->get_item_count(tab->user_data);
        }

        const bool is_list_tab =
            tab->ops != nullptr && tab->ops->get_content_height == nullptr;
        if (is_list_tab && item_count <= 0) {
            continue;
        }
        visible_tab_count++;
    }

    UI_TAB_SWITCH_TAB tab_switch_tabs[tab_count];
    UI_SETTINGS_TAB *visible_tabs = nullptr;
    if (visible_tab_count > 0) {
        visible_tabs =
            Memory_Alloc(sizeof(UI_SETTINGS_TAB) * visible_tab_count);
    }

    int32_t vt = 0;
    for (int32_t i = 0; i < tab_count; i++) {
        const UI_SETTINGS_TAB *const tab = &tabs[i];
        int32_t item_count = 0;
        if (tab->ops != nullptr && tab->ops->get_item_count != nullptr) {
            item_count = tab->ops->get_item_count(tab->user_data);
        }
        const bool is_list_tab =
            tab->ops != nullptr && tab->ops->get_content_height == nullptr;
        if (is_list_tab && item_count <= 0) {
            continue;
        }
        visible_tabs[vt] = tabs[i];
        tab_switch_tabs[vt].header.one_off = nullptr;
        tab_switch_tabs[vt].header.live_ptr =
            GameString_GetPtr(tabs[i].header_gs);
        vt++;
    }

    s->tabs = visible_tabs;
    s->tab_count = visible_tab_count;
    s->tab_switch = UI_TabSwitch_Init(s->tab_count, tab_switch_tabs);

    s->phase = UI_SETTINGS_PHASE_NAVIGATE_TABS;
    if (s->tab_count > 0) {
        M_SetActiveTab(s, 0);
    } else {
        M_RecomputeSizes(s);
    }

    return s;
}

void UI_SettingsDialog_Free(UI_SETTINGS_DIALOG_STATE *const s)
{
    if (s->listener_id >= 0) {
        GameStringManager_UnsubscribeReload(s->listener_id);
        s->listener_id = -1;
    }
    if (s->tab_switch != nullptr) {
        UI_TabSwitch_Free(s->tab_switch);
        s->tab_switch = nullptr;
    }
    if (s->tabs != nullptr) {
        for (int32_t i = 0; i < s->tab_count; i++) {
            if (s->tabs[i].ops != nullptr && s->tabs[i].ops->free != nullptr
                && s->tabs[i].user_data != nullptr) {
                s->tabs[i].ops->free(s->tabs[i].user_data);
            }
        }
        Memory_FreePointer(&s->tabs);
    }
    Memory_Free(s);
}

bool UI_SettingsDialog_Control(UI_SETTINGS_DIALOG_STATE *const s)
{
    M_RecomputeSizes(s);

    if (s->phase == UI_SETTINGS_PHASE_NAVIGATE_TABS) {
        if (UI_TabSwitch_Control(s->tab_switch, UI_TAB_SWITCH_NORMAL)) {
            M_SetActiveTab(s, s->tab_switch->active_tab_idx);
            return false;
        }
        if (g_InputDB.menu_down || g_InputDB.menu_confirm) {
            if (!g_InputDB.menu_confirm) {
                M_EnterEditMode(s, false);
            }
        } else if (g_InputDB.menu_up && g_Config.ui.enable_wraparound) {
            M_EnterEditMode(s, true);
        } else if (g_InputDB.menu_back) {
            return true;
        }
        return false;
    }

    UI_SETTINGS_TAB *const tab = M_GetActiveTab(s);
    if (tab == nullptr || tab->ops == nullptr) {
        return g_InputDB.menu_back;
    }

    const bool consumed = tab->ops->control(tab->user_data, &s->phase);

    if (s->phase == UI_SETTINGS_PHASE_NAVIGATE_TABS) {
        M_ClearActiveCustomSelection(s);
        return false;
    }

    if (consumed) {
        return false;
    }

    if (s->tab_switch != nullptr && !g_Input.menu_left && !g_Input.menu_right
        && UI_TabSwitch_Control(s->tab_switch, UI_TAB_SWITCH_NO_ARROWS)) {
        M_SetActiveTab(s, s->tab_switch->active_tab_idx);
        return false;
    }

    if (g_InputDB.menu_back) {
        return true;
    }

    return false;
}

void UI_SettingsDialog(UI_SETTINGS_DIALOG_STATE *const s)
{
    const UI_SETTINGS_TAB *const tab = M_GetActiveTabConst(s);
    UI_SCROLLABLE *const active_scroll = M_GetTabScrollable(M_GetActiveTab(s));

    UI_BeginModal(0.5f, 0.6f);
    UI_BeginStackEx((UI_STACK_SETTINGS) {
        .orientation = UI_STACK_VERTICAL,
        .spacing = { .v = 5.0f },
        .align = { .h = UI_STACK_H_ALIGN_SPAN },
    });

    UI_BeginWindow((UI_WINDOW_SETTINGS) {
        .title = GameString_Get(s->title),
        .scrollable = active_scroll,
        .title_spacing = -1.0f,
        .header_func = M_WindowHeader,
        .footer_func = nullptr,
        .user_data = s,
        .reserve_scroll_space = true,
    });

    if (tab == nullptr || tab->ops == nullptr || tab->ops->draw == nullptr) {
        UI_BeginResize(-1.0f, -1.0f);
        UI_BeginPad(
            g_TRVersion == 1 ? -1.0f : 0.0f, g_TRVersion == 1 ? -1.0f : 0.0f);
        UI_BeginStackEx((UI_STACK_SETTINGS) {
            .orientation = UI_STACK_VERTICAL,
            .align = { .h = UI_STACK_H_ALIGN_CENTER },
        });
        UI_Label(GS("general/settings/common/all_hidden_disclaimer"));
        UI_EndStack();
        UI_EndPad();
        UI_EndResize();
    } else {
        float content_width =
            s->max_content_width > 0.0f ? s->max_content_width : -1.0f;
        float content_height = s->max_content_height;
        UI_BeginResize(content_width, content_height);
        tab->ops->draw(tab->user_data, s->phase, s->max_content_width);
        UI_EndResize();
    }

    UI_EndWindow();
    if (tab != nullptr && tab->ops != nullptr
        && tab->ops->draw_footer != nullptr) {
        tab->ops->draw_footer(tab->user_data, s->phase);
    } else {
        UI_Spacer(0.0f, UI_TEXT_HEIGHT);
    }

    UI_EndStack();
    UI_EndModal();

    if (tab != nullptr && tab->ops != nullptr
        && tab->ops->draw_overlay != nullptr) {
        tab->ops->draw_overlay(tab->user_data);
    }
}
