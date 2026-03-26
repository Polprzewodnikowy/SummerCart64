#pragma once

#include <trx/config/types.h>
#include <trx/game/ui/common.h>

// state functions
void UI_LaraHealthBar_Control(void);
void UI_LaraHealthBar_SetTimer(int16_t timer);

// draw functions
bool UI_LaraHealthBar(bool blink_state, bool force);
