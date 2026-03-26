#pragma once

#include <trx/game/anims/types.h>
#include <trx/game/level/format/format.h>

#define NO_ANIM (-1)

void Anim_InitialiseAnims(int32_t num_anims);
void Anim_InitialiseChanges(int32_t num_changes);
void Anim_InitialiseRanges(int32_t num_ranges);
void Anim_InitialiseBones(int32_t num_bones);

void Anim_LoadCommands(const int16_t *data);

void Anim_InitialiseFrames(int32_t num_frames);
int32_t Anim_GetTotalFrameCount(
    const LEVEL_FORMAT_LOADER *loader, int32_t frame_data_length);
void Anim_LoadFrames(
    const LEVEL_FORMAT_LOADER *loader, const int16_t *data,
    int32_t data_length);

int32_t Anim_GetTotalCount(void);
ANIM *Anim_GetAnim(int32_t anim_idx);
ANIM_CHANGE *Anim_GetChange(int32_t change_idx);
ANIM_RANGE *Anim_GetRange(int32_t range_idx);
ANIM_BONE *Anim_GetBone(int32_t bone_idx);
ANIM_BONE *Anim_TryGetBone(int32_t bone_idx);

bool Anim_TestAbsFrameEqual(int16_t abs_frame, int16_t frame);
bool Anim_TestAbsFrameRange(int16_t abs_frame, int16_t start, int16_t end);
bool Anim_HasChange(const ANIM *anim, int16_t goal_state_id);
bool Anim_HasFXCommand(const ANIM *anim, int16_t fx_num);
bool Anim_HasFXCommandBetween(
    const ANIM *anim, int16_t fx_num, int32_t frame_a, int32_t frame_b);
