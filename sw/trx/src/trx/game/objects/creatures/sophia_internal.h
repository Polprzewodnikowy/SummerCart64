#pragma once

#include <trx/core/math/types.h>
#include <trx/game/items/types.h>

void Sophia_TriggerPlasmaBall(
    int32_t type, XYZ_32 pos, int16_t room_num, int16_t angle);
void Sophia_TriggerLaserBolt(
    XYZ_32 pos, const ITEM *item, int32_t type, int16_t angle);
