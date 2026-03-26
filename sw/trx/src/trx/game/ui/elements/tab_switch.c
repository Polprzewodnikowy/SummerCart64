#include <trx/game/ui/elements/tab_switch.h>

#include <trx/config.h>
#include <trx/core/memory.h>
#include <trx/game/input/common.h>
#include <trx/game/ui/elements/anchor.h>
#include <trx/game/ui/elements/frame.h>
#include <trx/game/ui/elements/label.h>
#include <trx/game/ui/elements/pad.h>
#include <trx/game/ui/elements/row_arrows.h>
#include <trx/game/ui/elements/stack.h>

static void M_Draw(
    const UI_TAB_SWITCH_STATE *const s, const bool is_focused,
    const bool single)
{
    UI_BeginAnchor(0.5f, 0.5f);
    UI_BeginRowArrows(
        is_focused && s->tab_count > 0
            && (g_Config.ui.enable_wraparound || s->active_tab_idx > 0),
        is_focused && s->tab_count > 0
            && (g_Config.ui.enable_wraparound
                || s->active_tab_idx + 1 < s->tab_count),
        UI_ROW_ARROWS_MEDIUM);
    UI_BeginStackEx((UI_STACK_SETTINGS) {
        .orientation = UI_STACK_HORIZONTAL,
        .align = { .h = UI_STACK_H_ALIGN_CENTER },
        .spacing = { .h = 10.0f },
    });
    for (int32_t i = 0; i < s->tab_count; i++) {
        if (single && i != s->active_tab_idx) {
            continue;
        }
        const UI_TAB_SWITCH_TAB *const tab = &s->tabs[i];
        UI_BeginAnchor(0.5f, 0.5f);
        if (i == s->active_tab_idx) {
            UI_BeginFrame(
                is_focused ? UI_FRAME_SELECTED_OPTION : UI_FRAME_OUTLINE_ONLY);
        }
        UI_BeginPad(2.0f, 1.0f);
        UI_Label(
            tab->header.live_ptr != nullptr ? *tab->header.live_ptr
                                            : tab->header.one_off);
        UI_EndPad();
        if (i == s->active_tab_idx) {
            UI_EndFrame();
        }
        UI_EndAnchor();
    }
    UI_EndStack();
    UI_EndRowArrows();
    UI_EndAnchor();
}

UI_TAB_SWITCH_STATE *UI_TabSwitch_Init(
    const int32_t tab_count, const UI_TAB_SWITCH_TAB *const tabs)
{
    UI_TAB_SWITCH_STATE *const s = Memory_Alloc(sizeof(UI_TAB_SWITCH_STATE));
    s->tab_count = tab_count;
    s->tabs = Memory_Dup(tabs, sizeof(UI_TAB_SWITCH_TAB) * tab_count);
    s->active_tab_idx = 0;
    return s;
}

void UI_TabSwitch_Free(UI_TAB_SWITCH_STATE *const s)
{
    Memory_Free(s->tabs);
    Memory_Free(s);
}

bool UI_TabSwitch_Cycle(UI_TAB_SWITCH_STATE *const s, const int32_t dir)
{
    if (s->tab_count == 0) {
        return false;
    } else if (s->active_tab_idx + dir < 0) {
        if (g_Config.ui.enable_wraparound) {
            s->active_tab_idx = s->tab_count - 1;
            return true;
        }
    } else if (s->active_tab_idx + dir >= s->tab_count) {
        if (g_Config.ui.enable_wraparound) {
            s->active_tab_idx = 0;
            return true;
        }
    } else {
        s->active_tab_idx += dir;
        return true;
    }
    return false;
}

bool UI_TabSwitch_Control(
    UI_TAB_SWITCH_STATE *const s, const UI_TAB_SWITCH_FLAGS flags)
{
    if ((!(flags & UI_TAB_SWITCH_NO_ARROWS) && g_InputDB.menu_left)
        || g_InputDB.step_left) {
        return UI_TabSwitch_Cycle(s, -1);
    } else if (
        (!(flags & UI_TAB_SWITCH_NO_ARROWS) && g_InputDB.menu_right)
        || g_InputDB.step_right) {
        return UI_TabSwitch_Cycle(s, 1);
    }
    return false;
}

void UI_TabSwitch(const UI_TAB_SWITCH_STATE *const s, const bool is_focused)
{
    M_Draw(s, is_focused, false);
}

void UI_TabSwitchSingle(
    const UI_TAB_SWITCH_STATE *const s, const bool is_focused)
{
    M_Draw(s, is_focused, true);
}
