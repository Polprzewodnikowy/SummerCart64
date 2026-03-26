#pragma once

#include <trx/core/colors.h>
#include <trx/core/math/types.h>

#include <stdint.h>

typedef struct {
    XZ_16 pos;
    RGB_888 color;
} FX_EXPLOSION_VERT;

typedef struct {
    int16_t on;
    int16_t life;
    int16_t speed;
    int16_t radius;
    int16_t prev_radius;
    XZ_16 rot;
    XZ_16 prev_rot;
    XYZ_32 pos;
    XYZ_32 prev_pos;
    FX_EXPLOSION_VERT verts[16];
} FX_RING;

typedef enum {
    FX_RING_TYPE_BLAST,
    FX_RING_TYPE_SUMMON,
    FX_RING_TYPE_KNOCKBACK,
    FX_RING_TYPE_NUMBER_OF,
} FX_RING_TYPE;

void FX_Ring_Reset(void);

void FX_Ring_Control(void);
void FX_Ring_Draw(void);
void FX_Ring_SpawnKnockBack(XYZ_32 pos);
void FX_Ring_BounceKnockBack(void);

void FX_Ring_Sync(FX_RING *ring);

bool FX_Ring_IsRingActive(FX_RING_TYPE type);
FX_RING *FX_Ring_GetRing(FX_RING_TYPE type, int32_t idx);
FX_RING *FX_Ring_PeekRing(FX_RING_TYPE type, int32_t idx);
