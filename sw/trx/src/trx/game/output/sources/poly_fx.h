#pragma once

#include <trx/core/colors.h>
#include <trx/game/output/shaders/mesh.h>
#include <trx/game/output/textures.h>
#include <trx/game/output/types.h>
#include <trx/game/sparks.h>
#include <trx/game/types.h>

#include <stdint.h>

void OutputSource_PolyFX_Init(void);
void OutputSource_PolyFX_Shutdown(void);

void OutputSource_PolyFX_StageSpriteQuadWorld(
    int32_t sprite_idx, const XYZ_32 world_pos[4], const RGBA_8888 color[4],
    DRAW_TYPE draw_type);

void OutputSource_PolyFX_StageSpriteQuadWorldDepth(
    int32_t sprite_idx, const XYZ_32 world_pos[4], const RGBA_8888 color[4],
    float z_depth_adjust, DRAW_TYPE draw_type);

void OutputSource_PolyFX_StageSpriteTriWorld(
    int32_t sprite_idx, const XYZ_32 world_pos[3], const RGBA_8888 color[3],
    DRAW_TYPE draw_type);

void OutputSource_PolyFX_StageSpriteTriWorldDepth(
    int32_t sprite_idx, const XYZ_32 world_pos[3], const RGBA_8888 color[3],
    float z_depth_adjust, DRAW_TYPE draw_type);

void OutputSource_PolyFX_StageQuadExt(
    int32_t sprite_idx, const XYZ_32 world_pos[4], const float disp[4][2],
    const RGBA_8888 color[4], uint16_t flags, DRAW_TYPE draw_type);

void OutputSource_PolyFX_StageQuadExtUV(
    const XYZ_32 world_pos[4], const OUTPUT_UVW uvw[4],
    const OUTPUT_TEXTURE_SIZE texture_size[4], const float disp[4][2],
    const RGBA_8888 color[4], uint16_t flags, DRAW_TYPE draw_type);

void OutputSource_PolyFX_StageTriExtUV(
    const XYZ_32 world_pos[3], const OUTPUT_UVW uvw[3],
    const OUTPUT_TEXTURE_SIZE texture_size[3], const float disp[3][2],
    const RGBA_8888 color[3], uint16_t flags, DRAW_TYPE draw_type);

void OutputSource_PolyFX_StageLineSegment(
    XYZ_32 from, RGBA_8888 from_color, XYZ_32 to, RGBA_8888 to_color,
    float half_width, DRAW_TYPE draw_type);

void OutputSource_PolyFX_StageSpark(const SPARK *spark);
