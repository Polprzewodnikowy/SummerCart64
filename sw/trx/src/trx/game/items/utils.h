#pragma once

#include <trx/game/items/types.h>

#define ITEM_ADJUST_ROT(source, target, rot)                                   \
    do {                                                                       \
        if ((int16_t)(target - source) > rot) {                                \
            source += rot;                                                     \
        } else if ((int16_t)(target - source) < -rot) {                        \
            source -= rot;                                                     \
        } else {                                                               \
            source = target;                                                   \
        }                                                                      \
    } while (0)

bool Item_IsTriggerActiveRO(const ITEM *item);
bool Item_IsTriggerActive(ITEM *item);

bool Item_IsAlive(const ITEM *item);
bool Item_IsTargetable(const ITEM *item);
bool Item_CanTakeDamage(const ITEM *item);
bool Item_CanBeProjectileTarget(const ITEM *item);

void Item_TakeDamage(ITEM *item, int16_t damage, bool hit_status);

bool Item_IsMeshVisible(const ITEM *item, int32_t mesh_num);
void Item_SetMeshVisible(ITEM *item, int32_t mesh_num, bool visible);
void Item_SetMeshVisibleMask(ITEM *item, uint32_t mesh_mask, bool visible);
void Item_ResetMeshBits(ITEM *item);

// Mesh_bits: which meshes to affect.
// Damage:
// * Positive values - deal damage, enable body part explosions.
// * Negative values - deal damage, disable body part explosions.
// * Zero - don't deal any damage, disable body part explosions.
int32_t Item_Explode(int16_t item_num, int32_t mesh_bits, int16_t damage);

bool Item_ShouldSpawnBlood(const ITEM *item);

int16_t Item_FindTypeInRoom(int16_t room_num, OBJECT_ID obj_id);
int16_t Item_FindTypeAtPos(int16_t room_num, XYZ_32 pos, OBJECT_ID obj_id);
