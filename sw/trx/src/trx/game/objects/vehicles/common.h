#pragma once

#include <trx/game/items/types.h>

int32_t Vehicle_DoShift(ITEM *vehicle, const XYZ_32 *pos, const XYZ_32 *old);
int32_t Vehicle_GetCollisionAnim(const ITEM *vehicle, XYZ_32 *moved);
