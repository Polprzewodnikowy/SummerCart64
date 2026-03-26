#pragma once

// Ingame user interface display widget.

#include <trx/game/objects/types.h>
#include <trx/game/ui/common.h>

typedef enum {
    UI_OVERLAY_TEXT_NONE = 0,
    UI_OVERLAY_TEXT_LITERAL,
    UI_OVERLAY_TEXT_GS_KEY,
    UI_OVERLAY_TEXT_OBJECT_NAME,
} UI_OVERLAY_TEXT_KIND;

typedef struct {
    UI_OVERLAY_TEXT_KIND kind;
    // Optional GS key of a %s format wrapper applied at draw time.
    const char *fmt_gs_key;
    bool flash_enabled;
    union {
        const char *literal;
        const char *gs_key;
        OBJECT_ID object_id;
    };
} UI_OVERLAY_TEXT;

typedef enum {
    UI_OVERLAY_ARROW_TL, // top-left screen corner
    UI_OVERLAY_ARROW_TR, // top-right screen corner
    UI_OVERLAY_ARROW_BL, // bottom-left screen corner
    UI_OVERLAY_ARROW_BR, // bottom-right screen corner
    UI_OVERLAY_ARROW_BCL, // low text left side
    UI_OVERLAY_ARROW_BCR, // low text right side
} UI_OVERLAY_ARROW;

typedef struct UI_OVERLAY_STATE UI_OVERLAY_STATE;

// state functions
UI_OVERLAY_STATE *UI_Overlay_Init(void);
void UI_Overlay_Free(UI_OVERLAY_STATE *s);
void UI_Overlay_Control(UI_OVERLAY_STATE *s);

// draw functions
void UI_Overlay(UI_OVERLAY_STATE *s);
void UI_BeginOverlayRegion(float x, float y);
void UI_EndOverlayRegion(void);

void UI_Overlay_ForceHealthBar(UI_OVERLAY_STATE *s, bool show);
void UI_Overlay_ShowArrow(
    UI_OVERLAY_STATE *s, UI_OVERLAY_ARROW arrow, bool show);
void UI_Overlay_ShowVersion(UI_OVERLAY_STATE *s, bool show);
void UI_Overlay_SetTopText(UI_OVERLAY_STATE *s, UI_OVERLAY_TEXT text);
void UI_Overlay_SetBottomText(UI_OVERLAY_STATE *s, UI_OVERLAY_TEXT text);
