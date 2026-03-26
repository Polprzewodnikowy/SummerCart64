#pragma once

#include <trx/core/virtual_file.h>

#include <stdint.h>

void Level_Section_AppendObjectMeshes(
    int32_t num_offsets, const int32_t *offsets, VFILE *file);
void Level_Section_AppendAnims(
    int32_t base_idx, int32_t num_anims, VFILE *file);
void Level_Section_AppendAnimChanges(
    int32_t base_idx, int32_t num_changes, VFILE *file);
void Level_Section_AppendAnimRanges(
    int32_t base_idx, int32_t num_ranges, VFILE *file);
void Level_Section_AppendAnimCommands(
    int32_t base_idx, int32_t num_commands, VFILE *file);
void Level_Section_AppendAnimBones(
    int32_t base_idx, int32_t num_bones, VFILE *file);
void Level_Section_AppendAnimFrames(
    int32_t base_idx, int32_t num_frames, VFILE *file);
void Level_Section_AppendObjectTextures(
    int32_t base_idx, int16_t base_page_idx, int32_t num_textures, VFILE *file,
    bool use_tr3_adjustment);
void Level_Section_AppendSpriteTextures(
    int32_t base_idx, int16_t base_page_idx, int32_t num_textures, VFILE *file);
