#pragma once

#include <trx/game/ui/common.h>

// Make the child widget invisible if the given flag is true.
// The children still take up their size.

void UI_BeginHide(bool hide_children);
void UI_EndHide(void);
