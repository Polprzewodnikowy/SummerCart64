#pragma once

#include <trx/game/collision/types.h>

int32_t Collide_GetSpheres(const ITEM *item, SPHERE *spheres, bool world_space);

int32_t Collide_TestCollision(ITEM *item, const ITEM *lara_item);

void Collide_GetJointAbsPosition(
    const ITEM *item, XYZ_32 *out_vec, int32_t joint);

void Collide_GetCollisionInfo(
    COLL_INFO *coll, int32_t x, int32_t y, int32_t z, int16_t room_num,
    int32_t obj_height);

bool Collide_CollideStaticObjects(
    COLL_INFO *coll, int32_t x, int32_t y, int32_t z, int16_t room_num,
    int32_t height);
bool Collide_TestBoundsCollide(
    const COLL_ITEM *src_item, const COLL_ITEM *dst_item, int32_t radius);

void Collide_DoProperDetection(ITEM *item, XYZ_32 old_pos);

void Collide_ShiftItem(ITEM *item, COLL_INFO *coll);
