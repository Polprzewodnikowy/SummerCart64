#pragma once

#include <trx/game/ui/common.h>

// Repetitive widget ops strategies
void UI_MeasureWrapper(UI_NODE *node);
void UI_LayoutBasic(UI_NODE *node, float x, float y, float w, float h);
void UI_LayoutWrapper(UI_NODE *node, float x, float y, float w, float h);
void UI_DrawWrapper(const UI_NODE *node);
