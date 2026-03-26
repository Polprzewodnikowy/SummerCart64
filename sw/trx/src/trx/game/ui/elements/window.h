#pragma once

#include <trx/game/ui/scrollable.h>

typedef void UI_WINDOW_CALLBACK(void *user_data);

typedef struct {
    const char *title;
    const UI_SCROLLABLE *scrollable;
    float title_spacing;
    bool heavy;
    UI_WINDOW_CALLBACK *header_func;
    UI_WINDOW_CALLBACK *footer_func;
    void *user_data;
    bool reserve_scroll_space;
} UI_WINDOW_SETTINGS;

void UI_BeginWindow(UI_WINDOW_SETTINGS settings);
void UI_EndWindow(void);
