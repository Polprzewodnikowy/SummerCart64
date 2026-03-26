#pragma once

#include <trx/game/items/types.h>

typedef struct {
    XYZ_32 pos[2];
    XYZ_32 prev_pos[2];
    XZ_32 vel[2];
    uint8_t life;
} FX_WAKE_POINT;

void FX_Wake_Reset(void);
void FX_Wake_Control(void);

FX_WAKE_POINT *FX_Wake_GetPoint(int32_t wake_idx, int32_t side);

uint8_t FX_Wake_GetShade(void);
void FX_Wake_SetShade(uint8_t shade);

uint8_t FX_Wake_GetStartIndex(void);
void FX_Wake_AdvanceStartIndex(void);

void FX_Wake_Draw(const ITEM *item);
