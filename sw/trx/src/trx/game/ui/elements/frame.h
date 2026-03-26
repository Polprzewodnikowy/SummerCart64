#pragma once

#include <trx/game/ui/common.h>

// A frame around the child widget.

typedef enum {
    UI_FRAME_DIALOG_BACKGROUND,
    UI_FRAME_DIALOG_BACKGROUND_HEAVY,
    UI_FRAME_DIALOG_HEADING,
    UI_FRAME_SELECTED_OPTION,
    UI_FRAME_OUTLINE_ONLY,
} UI_FRAME_STYLE;

void UI_BeginFrame(UI_FRAME_STYLE style);
void UI_EndFrame(void);
