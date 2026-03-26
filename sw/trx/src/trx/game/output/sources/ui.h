#pragma once

#include <trx/game/objects/types.h>
#include <trx/game/output/textures.h>
#include <trx/game/output/utils.h>
#include <trx/game/viewport.h>

#define OUTPUT_UI_MAX_PICKUP_ROWS 3
#define OUTPUT_UI_MAX_PICKUP_COLUMNS 4
#define OUTPUT_UI_MAX_PICKUPS                                                  \
    (OUTPUT_UI_MAX_PICKUP_COLUMNS * OUTPUT_UI_MAX_PICKUP_ROWS)

typedef struct {
    const OBJECT *object;
    int32_t grid_x;
    int32_t grid_y;
    int32_t rot_y;
    float ease;
} OUTPUT_UI_PICKUP;

typedef struct {
    int32_t sprite_idx;
    int32_t x0, y0;
    int32_t x1, y1;
    int32_t z;
    int16_t shade;
    RGBA_F color[4];
} OUTPUT_UI_SPRITE;

typedef struct {
    int32_t x0, y0;
    int32_t x1, y1;
    int32_t z;
    RGBA_8888 tl, tr, bl, br;
} OUTPUT_UI_QUAD;

void OutputSource_UI_Init(void);
void OutputSource_UI_Shutdown(void);

void OutputSource_UI_StagePickup(OUTPUT_UI_PICKUP pickup);
void OutputSource_UI_StageSprite(OUTPUT_UI_SPRITE sprite);
void OutputSource_UI_StageQuad(OUTPUT_UI_QUAD quad);
void OutputSource_UI_StagePhotoModeFrame(
    VIEWPORT_RECT rect, RGBA_8888 color, int32_t thickness);

VIEWPORT_RECT OutputSource_UI_GetPickupRect(const OUTPUT_UI_PICKUP *pickup);
