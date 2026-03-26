#include <trx/game/ui/elements/requester.h>

#include <trx/config.h>
#include <trx/core/utils.h>
#include <trx/game/input.h>
#include <trx/game/ui.h>
#include <trx/version.h>

void UI_Requester_Init(
    UI_REQUESTER_STATE *const s, const int32_t vis_rows, const int32_t max_rows,
    const bool is_selectable)
{
    s->scroll = (UI_SCROLLABLE) {
        .first_item = 0,
        .sel_item = 0,
        .vis_items = vis_rows,
        .max_items = max_rows,
    };
    s->is_selectable = is_selectable;
    s->row_pad = 20.0f;
    s->row_spacing = 3.0f;
    s->show_arrows = false;
    s->reserve_space = false;
}

void UI_Requester_Free(UI_REQUESTER_STATE *const s)
{
}

int32_t UI_Requester_Control(UI_REQUESTER_STATE *const s)
{
    if (s->is_selectable) {
        if (g_InputDB.menu_down) {
            UI_Scrollable_SelectNext(&s->scroll, g_Config.ui.enable_wraparound);
        } else if (g_InputDB.menu_up) {
            UI_Scrollable_SelectPrev(&s->scroll, g_Config.ui.enable_wraparound);
        }
    } else {
        if (g_InputDB.menu_down) {
            UI_Scrollable_ScrollDown(&s->scroll, g_Config.ui.enable_wraparound);
        } else if (g_InputDB.menu_up) {
            UI_Scrollable_ScrollUp(&s->scroll, g_Config.ui.enable_wraparound);
        }
    }

    if (s->is_selectable) {
        if (g_InputDB.menu_back) {
            return UI_REQUESTER_CANCEL;
        }
        if (g_InputDB.menu_confirm) {
            return s->scroll.sel_item;
        }
    }
    return UI_REQUESTER_NO_CHOICE;
}

void UI_Requester_SetMaxRows(UI_REQUESTER_STATE *const s, const size_t max_rows)
{
    UI_Scrollable_SetMaxItems(&s->scroll, max_rows);
}

void UI_Requester_SetVisibleRows(
    UI_REQUESTER_STATE *const s, const size_t visible_rows)
{
    UI_Scrollable_SetVisibleItems(&s->scroll, visible_rows);
}

int32_t UI_Requester_GetFirstRow(const UI_REQUESTER_STATE *const s)
{
    return UI_Scrollable_GetFirstVisibleItem(&s->scroll);
}

int32_t UI_Requester_GetLastRow(const UI_REQUESTER_STATE *const s)
{
    return UI_Scrollable_GetLastVisibleItem(&s->scroll) + 1;
}

int32_t UI_Requester_GetCurrentRow(const UI_REQUESTER_STATE *s)
{
    return UI_Scrollable_GetSelectedItem(&s->scroll);
}

bool UI_Requester_IsRowVisible(
    const UI_REQUESTER_STATE *const s, const int32_t i)
{
    return UI_Scrollable_IsItemVisible(&s->scroll, i);
}

bool UI_Requester_IsRowSelected(
    const UI_REQUESTER_STATE *const s, const int32_t i)
{
    return UI_Scrollable_IsItemSelected(&s->scroll, i);
}

void UI_Requester_SelectRow(UI_REQUESTER_STATE *const s, const int32_t i)
{
    UI_Scrollable_SelectItem(&s->scroll, i);
}

void UI_BeginRequester(
    const UI_REQUESTER_STATE *const s, const char *const title)
{
    const bool show_scroll_hints =
        s->show_arrows && s->scroll.vis_items < s->scroll.max_items;
    UI_BeginWindow((UI_WINDOW_SETTINGS) {
        .title = title,
        .scrollable = show_scroll_hints ? &s->scroll : nullptr,
        .title_spacing = -1.0f,
    });
    if (s->reserve_space) {
        UI_BeginResize(
            -1.0f,
            s->scroll.vis_items * UI_TEXT_HEIGHT
                + (s->scroll.vis_items - 1) * s->row_spacing);
    }

    UI_BeginStackEx((UI_STACK_SETTINGS) {
        .orientation = UI_STACK_VERTICAL,
        .align = { .h = UI_STACK_H_ALIGN_SPAN },
        .spacing = { .v = s->row_spacing },
    });
}

void UI_EndRequester(const UI_REQUESTER_STATE *const s)
{
    UI_EndStack();

    if (s->reserve_space) {
        UI_EndResize();
    }
    UI_EndWindow();
}

void UI_BeginRequesterRow(const UI_REQUESTER_STATE *const s, const int32_t i)
{
    UI_BeginPad(0.0f, g_TRVersion == 1 ? -1.0f : 0.0f);
    if (UI_Requester_IsRowSelected(s, i)) {
        UI_BeginFrame(UI_FRAME_SELECTED_OPTION);
    }
    UI_BeginPad(s->row_pad, g_TRVersion == 1 ? 1.0f : 0.0f);
}

void UI_EndRequesterRow(const UI_REQUESTER_STATE *const s, const int32_t i)
{
    UI_EndPad();
    if (UI_Requester_IsRowSelected(s, i)) {
        UI_EndFrame();
    }
    UI_EndPad();
}
