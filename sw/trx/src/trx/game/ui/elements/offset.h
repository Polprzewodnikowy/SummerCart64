#pragma once

#include <trx/game/ui/common.h>

// A transformer to move the child element in a certain direction.
// Does not affect the occupied space.

void UI_BeginOffset(float x, float y);
void UI_EndOffset(void);
