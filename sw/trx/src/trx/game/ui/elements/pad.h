#pragma once

#include <trx/game/ui/common.h>

// An invisible border in pixel units around the child widget.

void UI_BeginPad(float x, float y);
void UI_BeginPadEx(float l, float r, float t, float d);
void UI_EndPad(void);
