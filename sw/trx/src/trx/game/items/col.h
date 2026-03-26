#pragma once

#include <trx/game/items/types.h>
#include <trx/game/rooms/types.h>

int16_t Item_GetHeight(const ITEM *item);
int32_t Item_GetDistance(const ITEM *item, XYZ_32 target);
void Item_Translate(ITEM *item, int32_t x, int32_t y, int32_t z);
bool Item_Test3DRange(int32_t x, int32_t y, int32_t z, int32_t range);
bool Item_IsNearby(const ITEM *item_1, const ITEM *item_2, int32_t distance);

bool Item_TestBoundsCollide(
    const ITEM *src_item, const ITEM *dst_item, int32_t radius);
bool Item_TestStatic3DBoundsCollide(
    const STATIC_MESH *mesh, const ITEM *dst_item, int32_t radius);
const BOUNDS_16 *Item_GetBoundsAccurate(const ITEM *item);
BOUNDS_16 Item_RotateBounds(const ITEM *item, int16_t rot_y);
