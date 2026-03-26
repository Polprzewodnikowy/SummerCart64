#pragma once

#include <trx/game/output/mesh_batcher/batcher.h>
#include <trx/game/output/scene_source.h>
#include <trx/game/rooms/types.h>

void OutputSource_Sprites_Init(MESH_BATCHER *batcher);
void OutputSource_Sprites_Shutdown(void);
void OutputSource_Sprites_ObserveLevelLoad(void);
void OutputSource_Sprites_ObserveLevelUnload(void);

void OutputSource_Sprites_Stage(
    int32_t sprite_idx, int16_t shade, RGB_F tint, DRAW_TYPE draw_type);
