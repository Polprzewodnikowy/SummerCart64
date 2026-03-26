#pragma once

#include <trx/core/math.h>
#include <trx/game/anims.h>
#include <trx/game/collision.h>
#include <trx/game/items.h>
#include <trx/game/objects/ids.h>
#include <trx/game/objects/types.h>

bool Object_DrawUnclippedItem(const ITEM *item);
void Object_DrawMesh(int32_t mesh_idx, CLIP clip, bool interpolated);
bool Object_DrawSpriteItem(const ITEM *item);
bool Object_DrawPickupItem(const ITEM *item);
void Object_DrawStaticObject(const OBJECT *obj, const ANIM_FRAME *frame);

bool Object_DrawAnimatingItem(const ITEM *item);
bool Object_DrawAnimatingItemWithSwap(
    const ITEM *item, const OBJECT *mesh_swap);

bool Object_DrawInterpolatedObject(
    const OBJECT *obj, uint32_t mesh_mask, const int16_t *extra_rotation,
    const ANIM_FRAME *frame1, const ANIM_FRAME *frame2, int32_t frac,
    int32_t rate);
bool Object_DrawInterpolatedObjectWithSwap(
    const OBJECT *obj, uint32_t mesh_mask, const int16_t *extra_rotation,
    const ANIM_FRAME *frame1, const ANIM_FRAME *frame2, int32_t frac,
    int32_t rate, const OBJECT *mesh_swap);

void Object_ApplyExtraRotation(
    const int16_t **extra_rotation, const XYZ_BOOL rot_flags,
    bool interpolated);
