#pragma once

#include <trx/config/types.h>
#include <trx/game/objects/types.h>
#include <trx/game/output/types.h>
#include <trx/game/rooms/types.h>

void Output_DrawSkybox(const OBJECT_MESH *mesh);
void Output_DrawObjectMesh(const OBJECT_MESH *mesh, CLIP clip);
void Output_DrawObjectMesh_I(const OBJECT_MESH *mesh, CLIP clip);
void Output_DrawRoom(const ROOM *room, bool is_outside);

void Output_DrawSprite(
    int32_t x, int32_t y, int32_t z, int16_t sprnum, int16_t shade, RGB_F tint,
    DRAW_TYPE draw_type);
void Output_DrawShadow(int16_t size, const BOUNDS_16 *bounds, const ITEM *item);
void Output_DrawLightningSegment(const LIGHTNING_SEGMENT segment);

// Fades
void Output_DrawBlackRectangle(int32_t opacity);

// UI
void Output_DrawScreenSprite(
    int32_t sx, int32_t sy, int32_t sz, int32_t scale_h, int32_t scale_v,
    int32_t sprite_idx, const RGBA_F colors[4]);
void Output_DrawScreenFlatQuad(
    int32_t sx, int32_t sy, int32_t z, int32_t w, int32_t h, RGBA_8888 color);
void Output_DrawScreenGradientQuad(
    int32_t sx, int32_t sy, int32_t z, int32_t w, int32_t h, RGBA_8888 tl,
    RGBA_8888 tr, RGBA_8888 bl, RGBA_8888 br);
void Output_DrawScreenFrame(
    int32_t sx, int32_t sy, int32_t w, int32_t h, RGBA_8888 col_dark,
    RGBA_8888 col_light, float thickness);
void Output_DrawPhotoModeFrame(int32_t thickness);

void Output_DrawSphere(XYZ_16 center, int32_t radius);
void Output_DrawCuboid(const BOUNDS_16 *bounds);
void Output_DrawSphereEx(XYZ_16 center, int32_t radius, RGBA_8888 color);
void Output_DrawCuboidEx(const BOUNDS_16 *bounds, RGBA_8888 color);
