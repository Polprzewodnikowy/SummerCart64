#pragma once

#include <trx/game/types.h>

bool LOS_Check(
    const GAME_VECTOR *start, GAME_VECTOR *target, bool use_detailed_clipping);
int32_t LOS_CheckSmashable(
    GAME_VECTOR start, GAME_VECTOR target, XYZ_32 *out_hit_pos);
