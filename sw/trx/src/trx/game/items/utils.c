#include <trx/game/items/utils.h>

#include <trx/config.h>
#include <trx/core/utils.h>
#include <trx/game/creature.h>
#include <trx/game/effects.h>
#include <trx/game/matrix.h>
#include <trx/game/objects.h>
#include <trx/game/objects/vars.h>
#include <trx/game/output.h>
#include <trx/game/random.h>
#include <trx/version.h>

static bool M_UseTR3ExplodingEffects(const ITEM *const item)
{
    if (g_TRVersion < 3) {
        return false;
    }

    return !Object_IsType(item->object_id, g_ShatterableObjects)
        && !Object_IsType(item->object_id, g_HeavyShatterableObjects);
}

static bool M_IsFloating(const ITEM *const item)
{
    return Object_IsType(item->object_id, g_WaterObjects)
        && Object_Get(item->object_id)->intelligent && item->hit_points <= 0;
}

bool Item_IsAlive(const ITEM *const item)
{
    const OBJECT *const obj = Object_Get(item->object_id);
    if (obj->is_alive_func != nullptr) {
        return obj->is_alive_func(item);
    }

    if (obj->intelligent && Object_IsType(item->object_id, g_WaterObjects)) {
        return item->hit_points > 0;
    }
    return (item->hit_points > 0) || (item->active);
}

bool Item_IsTargetable(const ITEM *const item)
{
    const OBJECT *const obj = Object_Get(item->object_id);
    if (Object_IsType(item->object_id, g_ProjectileObjects)) {
        return false;
    }

    if (obj->is_targetable_func != nullptr) {
        return obj->is_targetable_func(item);
    }

    return item->hit_points > 0 && item->status == IS_ACTIVE
        && (g_Config.gameplay.enable_ally_targeting
            || Creature_IsHostile(item));
}

bool Item_CanTakeDamage(const ITEM *const item)
{
    const OBJECT *const obj = Object_Get(item->object_id);
    if (obj->can_take_damage_func != nullptr) {
        return obj->can_take_damage_func(item);
    }

    return Item_IsAlive(item) || M_IsFloating(item);
}

bool Item_CanBeProjectileTarget(const ITEM *const item)
{
    if (Object_IsType(item->object_id, g_ProjectileObjects)) {
        return false;
    }

    const OBJECT *const obj = Object_Get(item->object_id);
    if (obj->can_be_projectile_target_func != nullptr) {
        return obj->can_be_projectile_target_func(item);
    }

    if (Object_IsType(item->object_id, g_ShatterableObjects)
        || Object_IsType(item->object_id, g_SmashableObjects)) {
        return true;
    }

    if (M_IsFloating(item)) {
        return true;
    }

    if (!item->collidable || item->status == IS_INVISIBLE
        || obj->collision_func == nullptr) {
        return false;
    }

    return Item_IsTargetable(item);
}

void Item_TakeDamage(
    ITEM *const item, const int16_t damage, const bool hit_status)
{
    if (!Item_CanTakeDamage(item)) {
        return;
    }

    item->hit_points -= damage;
    CLAMPL(item->hit_points, 0);

    if (hit_status) {
        item->hit_status = true;
    }
}

bool Item_IsMeshVisible(const ITEM *const item, const int32_t mesh_num)
{
    if (mesh_num < 0 || mesh_num >= 32) {
        return false;
    }

    const uint32_t bit = 1u << mesh_num;
    return (item->mesh_bits & bit) != 0;
}

void Item_SetMeshVisibleMask(
    ITEM *const item, const uint32_t mesh_mask, const bool visible)
{
    if (visible) {
        item->mesh_bits |= mesh_mask;
    } else {
        item->mesh_bits &= ~mesh_mask;
    }
}

void Item_SetMeshVisible(
    ITEM *const item, const int32_t mesh_num, const bool visible)
{
    if (mesh_num < 0 || mesh_num >= 32) {
        return;
    }

    const uint32_t bit = 1u << mesh_num;
    Item_SetMeshVisibleMask(item, bit, visible);
}

void Item_ResetMeshBits(ITEM *const item)
{
    item->mesh_bits = UINT32_MAX;
}

int32_t Item_Explode(
    const int16_t item_num, const int32_t mesh_bits, const int16_t damage)
{
    ITEM *const item = Item_Get(item_num);
    const OBJECT *const obj = Object_Get(item->object_id);
    if (!obj->loaded) {
        return 0;
    }

    Output_CalculateLight(item->pos, item->room_num);

    const ANIM_FRAME *const best_frame = Item_GetBestFrame(item);

    Matrix_PushUnit();
    Matrix_Rot16(item->rot);
    Matrix_TranslateRel16(best_frame->offset);
    Matrix_Rot16(best_frame->mesh_rots[0]);

    const int32_t speed_shift = item->object_id == O_TORSO ? 7 : 8;
    const bool is_tr3 = M_UseTR3ExplodingEffects(item);

    // main mesh
    int32_t bit = 1;
    if ((mesh_bits & bit) && (item->mesh_bits & bit)) {
        const int16_t effect_num = Effect_Create(item->room_num);
        if (effect_num != NO_EFFECT) {
            EFFECT *const effect = Effect_Get(effect_num);
            effect->pos.x = item->pos.x + (g_MatrixPtr->_03 >> W2V_SHIFT);
            effect->pos.y = item->pos.y + (g_MatrixPtr->_13 >> W2V_SHIFT);
            effect->pos.z = item->pos.z + (g_MatrixPtr->_23 >> W2V_SHIFT);
            effect->rot.y = (Random_GetControl() - 0x4000) * 2;
            effect->room_num = item->room_num;
            effect->speed = Random_GetControl() >> speed_shift;
            effect->fall_speed = -Random_GetControl() >> speed_shift;
            effect->counter =
                is_tr3 ? ((damage << 2) | (Random_GetControl() & 3)) : damage;
            effect->object_id = O_BODY_PART;
            effect->frame_num = Object_GetItemMeshIndex(item, 0);
            effect->shade = Output_GetLightAdder() - 0x300;
        }
        item->mesh_bits &= ~bit;
    }

    // additional meshes
    const int16_t *extra_rotation = item->extra_rotations;
    for (int32_t i = 1; i < obj->mesh_count; i++) {
        const ANIM_BONE *const bone = Object_GetBone(obj, i - 1);
        if (bone->matrix_pop) {
            Matrix_Pop();
        }
        if (bone->matrix_push) {
            Matrix_Push();
        }

        Matrix_TranslateRel32(bone->pos);
        Matrix_Rot16(best_frame->mesh_rots[i]);
        Object_ApplyExtraRotation(&extra_rotation, bone->rot, false);

        bit <<= 1;
        if ((mesh_bits & bit) && (item->mesh_bits & bit)) {
            const int16_t effect_num = Effect_Create(item->room_num);
            if (effect_num != NO_EFFECT) {
                EFFECT *const effect = Effect_Get(effect_num);
                effect->pos.x = item->pos.x + (g_MatrixPtr->_03 >> W2V_SHIFT);
                effect->pos.y = item->pos.y + (g_MatrixPtr->_13 >> W2V_SHIFT);
                effect->pos.z = item->pos.z + (g_MatrixPtr->_23 >> W2V_SHIFT);
                effect->rot.y = (Random_GetControl() - 0x4000) * 2;
                effect->room_num = item->room_num;
                effect->speed = Random_GetControl() >> speed_shift;
                effect->fall_speed = -Random_GetControl() >> speed_shift;
                effect->counter = is_tr3
                    ? ((damage << 2) | (Random_GetControl() & 3))
                    : damage;
                effect->object_id = O_BODY_PART;
                effect->frame_num = Object_GetItemMeshIndex(item, i);
                effect->shade = Output_GetLightAdder() - 0x300;
            }
            item->mesh_bits &= ~bit;
        }
    }

    Matrix_Pop();

    return !(item->mesh_bits & (0x7FFFFFFF >> (31 - obj->mesh_count)));
}

bool Item_ShouldSpawnBlood(const ITEM *const item)
{
    if (item == nullptr) {
        return true;
    }

    const OBJECT *const obj = Object_Get(item->object_id);
    if (obj->should_spawn_blood_func != nullptr) {
        return obj->should_spawn_blood_func(item);
    }

    return true;
}

int16_t Item_FindTypeInRoom(const int16_t room_num, const OBJECT_ID obj_id)
{
    int16_t linked_item_num = Room_Get(room_num)->item_num;
    while (linked_item_num != NO_ITEM) {
        const ITEM *const linked_item = Item_Get(linked_item_num);
        if (linked_item->object_id == obj_id) {
            return linked_item_num;
        }
        linked_item_num = linked_item->next_item;
    }
    return NO_ITEM;
}

int16_t Item_FindTypeAtPos(
    const int16_t room_num, const XYZ_32 pos, const OBJECT_ID obj_id)
{
    const ROOM *const room = Room_Get(room_num);
    int16_t item_num = room->item_num;
    while (item_num != NO_ITEM) {
        const ITEM *const item = Item_Get(item_num);
        if (item->object_id == obj_id && XYZ_32_AreEquivalent(item->pos, pos)) {
            return item_num;
        }
        item_num = item->next_item;
    }
    return NO_ITEM;
}

bool Item_IsTriggerActiveRO(const ITEM *const item)
{
    const bool ok = (item->flags & IF_REVERSE) == 0;
    if ((item->flags & IF_CODE_BITS) != IF_CODE_BITS) {
        return !ok;
    }
    if (item->timer == 0) {
        return ok;
    }
    if (item->timer == -1) {
        return !ok;
    }
    return ok;
}

bool Item_IsTriggerActive(ITEM *const item)
{
    const bool result = Item_IsTriggerActiveRO(item);
    if (item->timer != 0 && item->timer != -1) {
        item->timer--;
        if (item->timer == 0) {
            item->timer = -1;
        }
    }
    return result;
}
