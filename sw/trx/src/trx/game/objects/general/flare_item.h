#pragma once

#include <trx/core/math.h>
#include <trx/game/objects/types.h>

void Flare_GenerateEffects(
    const XYZ_32 *sound_pos, XYZ_32 flare_pos, int16_t room_num);
bool Flare_GenerateLight(XYZ_32 pos, int32_t flare_age);
int32_t Flare_GetMaxAge(void);
int32_t FlareItem_GetAge(const ITEM *item);
bool FlareItem_IsActive(const ITEM *item);
void FlareItem_SetAge(ITEM *item, int32_t flare_age, bool is_active);
