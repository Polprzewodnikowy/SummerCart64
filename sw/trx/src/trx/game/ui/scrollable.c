#include <trx/game/ui/scrollable.h>

#include <trx/config.h>
#include <trx/core/utils.h>

static void M_Clamp(UI_SCROLLABLE *const s, const bool include_selected_item)
{
    if (include_selected_item && s->sel_item != -1) {
        CLAMP(s->first_item, s->sel_item - s->vis_items + 1, s->sel_item);
    }
    CLAMPG(s->first_item, s->max_items - s->vis_items);
    CLAMPL(s->first_item, 0);
}

bool UI_Scrollable_SelectNext(
    UI_SCROLLABLE *const s, const bool enable_wraparound)
{
    if (s->sel_item + 1 < s->max_items) {
        s->sel_item++;
    } else if (enable_wraparound) {
        s->sel_item = 0;
    } else {
        return false;
    }
    M_Clamp(s, true);
    return true;
}

bool UI_Scrollable_SelectPrev(
    UI_SCROLLABLE *const s, const bool enable_wraparound)
{
    if (s->sel_item > 0) {
        s->sel_item--;
    } else if (enable_wraparound) {
        s->sel_item = s->max_items - 1;
    } else {
        return false;
    }
    M_Clamp(s, true);
    return true;
}

bool UI_Scrollable_ScrollDown(
    UI_SCROLLABLE *const s, const bool enable_wraparound)
{
    if (s->first_item + 1 <= s->max_items - s->vis_items) {
        s->first_item++;
    } else if (enable_wraparound) {
        s->first_item = 0;
    } else {
        return false;
    }
    M_Clamp(s, false);
    return true;
}

bool UI_Scrollable_ScrollUp(
    UI_SCROLLABLE *const s, const bool enable_wraparound)
{
    if (s->first_item > 0) {
        s->first_item--;
    } else if (enable_wraparound) {
        s->first_item = s->max_items - 1;
    } else {
        return false;
    }
    M_Clamp(s, false);
    return true;
}

void UI_Scrollable_SetVisibleItems(
    UI_SCROLLABLE *const s, const int32_t visible_items)
{
    s->vis_items = visible_items;
    CLAMPL(s->vis_items, 0);
    M_Clamp(s, true);
}

void UI_Scrollable_SetMaxItems(UI_SCROLLABLE *const s, const int32_t max_items)
{
    s->max_items = max_items;
    CLAMP(s->sel_item, 0, s->max_items - 1);
}

void UI_Scrollable_SelectItem(UI_SCROLLABLE *const s, const int32_t row)
{
    s->sel_item = row;
    if (s->sel_item != -1) {
        CLAMP(s->sel_item, 0, s->max_items - 1);
        CLAMP(s->first_item, s->sel_item - s->vis_items + 1, s->sel_item);
    }
}

void UI_Scrollable_SelectFirstItem(UI_SCROLLABLE *const s)
{
    UI_Scrollable_SelectItem(s, 0);
}

void UI_Scrollable_SelectLastItem(UI_SCROLLABLE *const s)
{
    UI_Scrollable_SelectItem(s, s->max_items - 1);
}

int32_t UI_Scrollable_GetFirstVisibleItem(const UI_SCROLLABLE *const s)
{
    return s->first_item;
}

int32_t UI_Scrollable_GetSelectedItem(const UI_SCROLLABLE *const s)
{
    return s->sel_item;
}

int32_t UI_Scrollable_GetLastVisibleItem(const UI_SCROLLABLE *const s)
{
    return MIN(s->first_item + s->vis_items - 1, s->max_items - 1);
}

bool UI_Scrollable_IsItemVisible(
    const UI_SCROLLABLE *const s, const int32_t item)
{
    return item >= UI_Scrollable_GetFirstVisibleItem(s)
        && item <= UI_Scrollable_GetLastVisibleItem(s);
}

bool UI_Scrollable_IsItemSelected(
    const UI_SCROLLABLE *const s, const int32_t item)
{
    return item == UI_Scrollable_GetSelectedItem(s);
}
