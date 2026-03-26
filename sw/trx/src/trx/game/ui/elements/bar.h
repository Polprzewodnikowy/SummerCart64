#pragma once

#include <trx/config/types.h>
#include <trx/game/ui/common.h>
#include <trx/game/ui/settings.h>
#include <trx/version.h>

// shared properties of common ingame bars
#define UI_BAR_WIDTH 208.0f
#define UI_BAR_HEIGHT 18.0f
#define UI_BAR_BORDER 2.0f
#define UI_BAR_PADDING 2.0f
#define UI_BAR_BLINK_THRESHOLD (g_TRVersion == 1 ? 0.2f : 0.25f)

typedef struct {
    UI_BAR_TYPE type;
    int32_t w;
    int32_t h;
    int32_t value;
    int32_t max_value;
    bool preview;
} UI_BAR_SETTINGS;

// draw functions
void UI_Bar(UI_BAR_SETTINGS settings);
