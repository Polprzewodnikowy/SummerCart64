#pragma once

#include <stdint.h>

typedef struct {
    int32_t first_item;
    int32_t sel_item;
    int32_t max_items;
    int32_t vis_items;
} UI_SCROLLABLE;

bool UI_Scrollable_SelectNext(UI_SCROLLABLE *s, bool enable_wraparound);
bool UI_Scrollable_SelectPrev(UI_SCROLLABLE *s, bool enable_wraparound);
bool UI_Scrollable_ScrollDown(UI_SCROLLABLE *s, bool enable_wraparound);
bool UI_Scrollable_ScrollUp(UI_SCROLLABLE *s, bool enable_wraparound);

void UI_Scrollable_SetVisibleItems(UI_SCROLLABLE *s, int32_t visible_items);
void UI_Scrollable_SetMaxItems(UI_SCROLLABLE *s, int32_t max_items);
void UI_Scrollable_SelectItem(UI_SCROLLABLE *s, int32_t item);
void UI_Scrollable_SelectFirstItem(UI_SCROLLABLE *s);
void UI_Scrollable_SelectLastItem(UI_SCROLLABLE *s);

int32_t UI_Scrollable_GetFirstVisibleItem(const UI_SCROLLABLE *s);
int32_t UI_Scrollable_GetLastVisibleItem(const UI_SCROLLABLE *s);
int32_t UI_Scrollable_GetSelectedItem(const UI_SCROLLABLE *s);
bool UI_Scrollable_IsItemVisible(const UI_SCROLLABLE *s, int32_t item);
bool UI_Scrollable_IsItemSelected(const UI_SCROLLABLE *s, int32_t item);
