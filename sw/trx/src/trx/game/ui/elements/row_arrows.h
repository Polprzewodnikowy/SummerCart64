#pragma once

#include <trx/game/ui/common.h>

#define UI_ROW_ARROWS_TIGHT 2.0f
#define UI_ROW_ARROWS_MEDIUM 5.0f
#define UI_ROW_ARROWS_WIDE 15.0f

// Conditionally display a left arrow and a right arrow around the child widget
// with the given spacing.

void UI_BeginRowArrows(bool left_arrow, bool right_arrow, int32_t spacing);
void UI_EndRowArrows(void);
