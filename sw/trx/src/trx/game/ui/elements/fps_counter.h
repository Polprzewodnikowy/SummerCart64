#pragma once

#include <trx/game/ui/common.h>

typedef struct UI_FPS_COUNTER_STATE UI_FPS_COUNTER_STATE;

// state functions
UI_FPS_COUNTER_STATE *UI_FPSCounter_Init(void);
void UI_FPSCounter_Free(UI_FPS_COUNTER_STATE *s);

// draw functions
void UI_FPSCounter(UI_FPS_COUNTER_STATE *s);
