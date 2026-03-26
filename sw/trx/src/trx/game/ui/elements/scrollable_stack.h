#pragma once

#include <trx/game/ui/elements/stack.h>
#include <trx/game/ui/scrollable.h>

#include <stdint.h>

typedef struct {
    UI_STACK_ORIENTATION orientation;
    float spacing;
} UI_SCROLLABLE_STACK_SETTINGS;

bool UI_ScrollableStack_Control(
    UI_SCROLLABLE *s, UI_STACK_ORIENTATION orientation);

void UI_BeginScrollableStack(
    UI_SCROLLABLE *s, UI_SCROLLABLE_STACK_SETTINGS settings);
void UI_EndScrollableStack(void);
