#pragma once

#include <trx/game/objects/types.h>
#include <trx/game/ui/hud/overlay.h>

typedef UI_OVERLAY_TEXT OVERLAY_TEXT;

void Overlay_Init(void);
void Overlay_Shutdown(void);

void Overlay_Reset(void);
void Overlay_Control(void);
void Overlay_Animate(int32_t num_frames);
void Overlay_Draw(void);

void Overlay_DrawGameInfo(void);
void Overlay_AddDisplayPickup(OBJECT_ID obj_id);

void Overlay_ForceHealthBar(bool show);
void Overlay_SetHealthBarTimer(int16_t health_bar_timer);
void Overlay_ShowArrow(UI_OVERLAY_ARROW arrow, bool show);
void Overlay_ShowVersion(bool show);

void Overlay_SetTopText(OVERLAY_TEXT text);
void Overlay_SetBottomText(OVERLAY_TEXT text);
