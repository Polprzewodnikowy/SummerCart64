#include <trx/game/gun/misc.h>

#include <trx/config.h>
#include <trx/core/math.h>
#include <trx/game/creature.h>
#include <trx/game/gun/common.h>
#include <trx/game/gun/pistols.h>
#include <trx/game/gun/rifle.h>
#include <trx/game/gun/vars.h>
#include <trx/game/input.h>
#include <trx/game/inventory.h>
#include <trx/game/items.h>
#include <trx/game/lara.h>
#include <trx/game/los.h>
#include <trx/game/matrix.h>
#include <trx/game/output.h>
#include <trx/game/pathing.h>
#include <trx/game/random.h>
#include <trx/game/sound.h>
#include <trx/game/spawn.h>
#include <trx/game/stats.h>

#define M_NEAR_ANGLE (DEG_1 * 15) // = 2730

static ITEM *m_TargetList[LOT_SLOT_COUNT] = {};
static ITEM *m_LastTargetList[LOT_SLOT_COUNT] = {};
static int16_t m_TargetCount = 0;

static bool M_TargetListContains(const ITEM *const item, const int16_t count)
{
    for (int16_t i = 0; i < count; i++) {
        if (m_TargetList[i] == item) {
            return true;
        }
    }
    return false;
}

typedef struct {
    const WEAPON_INFO *weapon;
    const GAME_VECTOR *start;
    const ITEM *lara_item;
    const LARA_INFO *lara;
    const ITEM *old_target;
    int32_t max_dist;
    ITEM *best_target;
    int16_t best_y_rot;
    int32_t best_dist;
    int16_t num_targets;
    int32_t old_target_dist;
    int16_t old_target_y_rot;
    bool old_target_in_list;
} M_TARGET_CONTEXT;

static void M_ConsiderTarget(M_TARGET_CONTEXT *const ctx, ITEM *const item)
{
    if (item == ctx->lara_item || !Item_IsTargetable(item)) {
        return;
    }

    const int32_t dx = item->pos.x - ctx->start->x;
    const int32_t dy = item->pos.y - ctx->start->y;
    const int32_t dz = item->pos.z - ctx->start->z;
    if (ABS(dx) > ctx->max_dist || ABS(dy) > ctx->max_dist
        || ABS(dz) > ctx->max_dist) {
        return;
    }

    const int32_t dist = SQUARE(dx) + SQUARE(dy) + SQUARE(dz);
    if (dist >= SQUARE(ctx->max_dist)) {
        return;
    }

    GAME_VECTOR target;
    Gun_FindTargetPoint(item, &target);
    if (!LOS_Check(ctx->start, &target, true)) {
        return;
    }

    int16_t angles[2];
    Math_GetVectorAngles(
        target.x - ctx->start->x, target.y - ctx->start->y,
        target.z - ctx->start->z, angles);
    angles[0] -= ctx->lara->torso_rot.y + ctx->lara_item->rot.y;
    angles[1] -= ctx->lara->torso_rot.x + ctx->lara_item->rot.x;

    if (angles[0] < ctx->weapon->lock_angles[0]
        || angles[0] > ctx->weapon->lock_angles[1]
        || angles[1] < ctx->weapon->lock_angles[2]
        || angles[1] > ctx->weapon->lock_angles[3]) {
        return;
    }

    if (ctx->num_targets < LOT_SLOT_COUNT) {
        m_TargetList[ctx->num_targets] = item;
        ctx->num_targets++;
    }

    const int16_t y_rot = ABS(angles[0]);
    if (item == ctx->old_target) {
        ctx->old_target_dist = dist;
        ctx->old_target_y_rot = y_rot;
        ctx->old_target_in_list = true;
    }

    if (g_TRVersion == 1) {
        if (y_rot < ctx->best_y_rot) {
            ctx->best_dist = dist;
            ctx->best_y_rot = y_rot;
            ctx->best_target = item;
        }
    } else {
        if (y_rot < ctx->best_y_rot + M_NEAR_ANGLE && dist < ctx->best_dist) {
            ctx->best_dist = dist;
            ctx->best_y_rot = y_rot;
            ctx->best_target = item;
        }
    }
}

static void M_DrawGunGlow(const XYZ_32 offset, const RGB_F color)
{
    if (g_TRVersion < 3) {
        return;
    }
    const OBJECT *const glow_obj = Object_Get(O_GLOW);
    if (!glow_obj->loaded) {
        return;
    }

    Matrix_Push();
    Matrix_TranslateRel32(offset);
    const XYZ_32 pos = {
        .x = (int32_t)(g_WMatrixPtr->_03 >> W2V_SHIFT),
        .y = (int32_t)(g_WMatrixPtr->_13 >> W2V_SHIFT),
        .z = (int32_t)(g_WMatrixPtr->_23 >> W2V_SHIFT),
    };
    Matrix_Pop();
    Output_DrawSprite(
        pos.x, pos.y, pos.z, glow_obj->mesh_idx, 0, color, DRAW_BLEND_ADD);
}

void Gun_FindTargetPoint(const ITEM *const item, GAME_VECTOR *const target)
{
    const BOUNDS_16 *const bounds = &Item_GetBestFrame(item)->bounds;
    const int32_t x = bounds->min.x + (bounds->max.x - bounds->min.x) / 2;
    const int32_t y = bounds->min.y + (bounds->max.y - bounds->min.y) / 3;
    const int32_t z = bounds->min.z + (bounds->max.z - bounds->min.z) / 2;
    const int32_t cy = Math_Cos(item->rot.y);
    const int32_t sy = Math_Sin(item->rot.y);
    target->pos.x = item->pos.x + ((cy * x + sy * z) >> W2V_SHIFT);
    target->pos.y = item->pos.y + y;
    target->pos.z = item->pos.z + ((cy * z - sy * x) >> W2V_SHIFT);
    target->room_num = item->room_num;
}

void Gun_AimWeapon(const WEAPON_INFO *const weapon, LARA_ARM *const arm)
{
    const LARA_INFO *const lara = Lara_GetLaraInfo();
    const int16_t speed = weapon->aim_speed;

    int16_t dest_x = 0;
    int16_t dest_y = 0;
    if (arm->lock) {
        dest_y = lara->target_angles[0];
        dest_x = lara->target_angles[1];
    }

    if (arm->rot.y >= dest_y - speed && arm->rot.y <= dest_y + speed) {
        arm->rot.y = dest_y;
    } else if (arm->rot.y < dest_y) {
        arm->rot.y += speed;
    } else {
        arm->rot.y -= speed;
    }

    if (arm->rot.x >= dest_x - speed && arm->rot.x <= dest_x + speed) {
        arm->rot.x = dest_x;
    } else if (arm->rot.x < dest_x) {
        arm->rot.x += speed;
    } else {
        arm->rot.x -= speed;
    }

    arm->rot.z = 0;
}

void Gun_TargetInfo(const WEAPON_INFO *const weapon)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    const ITEM *const lara_item = Lara_GetItem();
    if (lara->target == nullptr) {
        lara->left_arm.lock = 0;
        lara->right_arm.lock = 0;
        lara->target_angles[0] = 0;
        lara->target_angles[1] = 0;
        return;
    }

    GAME_VECTOR target;
    GAME_VECTOR start = {
        .pos = {
            .x = lara_item->pos.x,
            .y = lara_item->pos.y - 650,
            .z = lara_item->pos.z,
        },
        .room_num = lara_item->room_num,
    };
    Gun_FindTargetPoint(lara->target, &target);

    int16_t angles[2];
    // clang-format off
    Math_GetVectorAngles(
        target.pos.x - start.pos.x,
        target.pos.y - start.pos.y,
        target.pos.z - start.pos.z,
        angles);
    // clang-format on

    angles[0] -= lara_item->rot.y;
    angles[1] -= lara_item->rot.x;

    if (!LOS_Check(&start, &target, true)) {
        lara->left_arm.lock = 0;
        lara->right_arm.lock = 0;
    } else if (
        angles[0] >= weapon->lock_angles[0]
        && angles[0] <= weapon->lock_angles[1]
        && angles[1] >= weapon->lock_angles[2]
        && angles[1] <= weapon->lock_angles[3]) {
        lara->left_arm.lock = 1;
        lara->right_arm.lock = 1;
    } else {
        if (lara->left_arm.lock
            && (angles[0] < weapon->left_angles[0]
                || angles[0] > weapon->left_angles[1]
                || angles[1] < weapon->left_angles[2]
                || angles[1] > weapon->left_angles[3])) {
            lara->left_arm.lock = 0;
        }
        if (lara->right_arm.lock
            && (angles[0] < weapon->right_angles[0]
                || angles[0] > weapon->right_angles[1]
                || angles[1] < weapon->right_angles[2]
                || angles[1] > weapon->right_angles[3])) {
            lara->right_arm.lock = 0;
        }
    }

    lara->target_angles[0] = angles[0];
    lara->target_angles[1] = angles[1];
}

void Gun_InitialiseNewWeapon(void)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->target = nullptr;

    lara->left_arm.flash_gun = 0;
    lara->left_arm.frame_num = LF_G_AIM_START;
    lara->left_arm.lock = 0;
    lara->left_arm.rot.x = 0;
    lara->left_arm.rot.y = 0;
    lara->left_arm.rot.z = 0;

    lara->right_arm.flash_gun = 0;
    lara->right_arm.frame_num = LF_G_AIM_START;
    lara->right_arm.lock = 0;
    lara->right_arm.rot.x = 0;
    lara->right_arm.rot.y = 0;
    lara->right_arm.rot.z = 0;

    const OBJECT_ID anim_type = Gun_GetLaraAnim(lara->gun_type);
    const OBJECT *const obj = Object_Get(anim_type);
    lara->left_arm.frame_base = obj->frame_base;
    lara->right_arm.frame_base = obj->frame_base;

    if (lara->gun_status != LGS_ARMLESS) {
        switch (lara->gun_type) {
        case LGT_PISTOLS:
        case LGT_MAGNUMS:
        case LGT_AUTOS:
        case LGT_DESERT_EAGLE:
        case LGT_UZIS:
            Gun_Pistols_DrawMeshes(lara->gun_type);
            break;

        case LGT_SHOTGUN:
        case LGT_M16:
        case LGT_MP5:
        case LGT_GRENADE:
        case LGT_ROCKET:
        case LGT_HARPOON:
            Gun_Rifle_DrawMeshes(lara->gun_type);
            break;

        case LGT_FLARE:
            Lara_Flare_DrawMeshes();
            break;

        default:
            break;
        }
    }
}

void Gun_DrawFlash(
    const LARA_GUN_TYPE weapon_type, const CLIP clip, const bool interpolated)
{
    if (weapon_type == LGT_SHOTGUN && !g_Config.visuals.enable_shotgun_flash) {
        return;
    }

    OBJECT_ID flash_object_id = O_GUN_FLASH;
    XYZ_16 rot = {};

    switch (weapon_type) {
    case LGT_M16:
    case LGT_MP5:
        rot.x = -85 * DEG_1;
        rot.z = ((2 * Random_GetDraw()) & 0x4000);
        if (weapon_type == LGT_M16) {
            rot.z += 0x2000;
        } else {
            rot.z += (Random_GetDraw() & 0xFFF) + 0x1800;
        }
        flash_object_id = O_M16_FLASH;
        break;

    case LGT_FLARE:
        rot.x = -DEG_90;
        rot.y = 2 * Random_GetDraw();
        flash_object_id = O_FLARE_FIRE;
        break;

    default:
        rot.x = -DEG_90;
        rot.z = 2 * Random_GetDraw();
        break;
    }

    const WEAPON_INFO weapon = g_Weapons[weapon_type];
    if (interpolated) {
        Matrix_TranslateRel32_I(weapon.flash_pos);
        Matrix_RotX_I(rot.x);
        Matrix_RotY_I(rot.y);
        Matrix_RotZ_I(rot.z);
    } else {
        Matrix_TranslateRel32(weapon.flash_pos);
        Matrix_RotX(rot.x);
        Matrix_RotY(rot.y);
        Matrix_RotZ(rot.z);
    }

    const GAME_VECTOR pos = {
        .room_num = Lara_GetItem()->room_num,
        .pos = Matrix_MulVec32_M(g_WMatrixPtr, (XYZ_32) {}),
    };
    Output_PushTintOverride(Lara_GetMeshTint(pos));

    if (g_TRVersion < 3) {
        Output_CalculateStaticLight(weapon.flash_shade);
    } else {
        Output_CalculateStaticLightRGB_F(weapon.flash_color);
    }
    const OBJECT *const flash_obj = Object_Get(flash_object_id);
    if (flash_obj->loaded) {
        Object_DrawMesh(flash_obj->mesh_idx, clip, interpolated);
    }

    M_DrawGunGlow(weapon.glow_pos, weapon.glow_color);
    Output_PopTintOverride();
}

void Gun_UpdateLaraMeshes(const OBJECT_ID obj_id)
{
    const bool lara_has_rifle = Inv_RequestItem(O_SHOTGUN_ITEM)
        || Inv_RequestItem(O_HARPOON_ITEM) || Inv_RequestItem(O_M16_ITEM)
        || Inv_RequestItem(O_MP5_ITEM) || Inv_RequestItem(O_GRENADE_GUN_ITEM)
        || Inv_RequestItem(O_ROCKET_GUN_ITEM);
    const bool lara_has_pistols = Inv_RequestItem(O_PISTOL_ITEM)
        || Inv_RequestItem(O_MAGNUM_ITEM) || Inv_RequestItem(O_AUTOS_ITEM)
        || Inv_RequestItem(O_DESERT_EAGLE_ITEM) || Inv_RequestItem(O_UZI_ITEM);

    LARA_GUN_TYPE back_gun_type = LGT_UNARMED;
    LARA_GUN_TYPE holsters_gun_type = LGT_UNARMED;

    if (!lara_has_rifle && obj_id == O_SHOTGUN_ITEM) {
        back_gun_type = LGT_SHOTGUN;
    } else if (!lara_has_rifle && obj_id == O_HARPOON_ITEM) {
        back_gun_type = LGT_HARPOON;
    } else if (!lara_has_rifle && obj_id == O_M16_ITEM) {
        back_gun_type = LGT_M16;
    } else if (!lara_has_rifle && obj_id == O_MP5_ITEM) {
        back_gun_type = LGT_MP5;
    } else if (!lara_has_rifle && obj_id == O_GRENADE_GUN_ITEM) {
        back_gun_type = LGT_GRENADE;
    } else if (!lara_has_rifle && obj_id == O_ROCKET_GUN_ITEM) {
        back_gun_type = LGT_ROCKET;
    } else if (!lara_has_pistols && obj_id == O_PISTOL_ITEM) {
        holsters_gun_type = LGT_PISTOLS;
    } else if (!lara_has_pistols && obj_id == O_MAGNUM_ITEM) {
        holsters_gun_type = LGT_MAGNUMS;
    } else if (!lara_has_pistols && obj_id == O_AUTOS_ITEM) {
        holsters_gun_type = LGT_AUTOS;
    } else if (!lara_has_pistols && obj_id == O_DESERT_EAGLE_ITEM) {
        holsters_gun_type = LGT_DESERT_EAGLE;
    } else if (!lara_has_pistols && obj_id == O_UZI_ITEM) {
        holsters_gun_type = LGT_UZIS;
    }

    if (back_gun_type != LGT_UNARMED) {
        Gun_SetLaraBackMesh(back_gun_type);
    }

    if (holsters_gun_type != LGT_UNARMED) {
        Gun_SetLaraHolsterLMesh(holsters_gun_type);
        Gun_SetLaraHolsterRMesh(holsters_gun_type);
    }
}

void Gun_HitTarget(
    ITEM *const item, const GAME_VECTOR *const start,
    const GAME_VECTOR *const hit_pos, int32_t damage)
{
    OBJECT *const obj = Object_Get(item->object_id);
    if (obj->gun_hit_func != nullptr) {
        const bool use_default =
            obj->gun_hit_func(item, start, hit_pos, &damage);
        if (!use_default) {
            return;
        }
    }

    const bool make_ricochet = !Item_ShouldSpawnBlood(item);
    if (item->object_id == O_SHIVA && make_ricochet) {
        damage = 0;
    }

    LARA_INFO *const lara = Lara_GetLaraInfo();
    const bool was_alive = item->hit_points > 0;
    const bool clears_target = was_alive && damage >= item->hit_points;
    if (clears_target) {
        if (item->include_in_kill_stats) {
            Stats_AddKill();
        }
        if (g_Config.gameplay.target_mode == TARGET_LOCK_MODE_SEMI) {
            lara->target = nullptr;
        }
    }

    Item_TakeDamage(item, damage, true);
    if (item->creature_data != nullptr
        && Object_Get(item->object_id)->intelligent) {
        Creature_Hurt(item, damage);
    }

    if (hit_pos != nullptr) {
        if (make_ricochet) {
            const GAME_VECTOR pos = {
                .pos = hit_pos->pos,
                .room_num = item->room_num,
            };
            if (start != nullptr) {
                Spawn_RicochetRay(*start, pos);
            } else {
                Spawn_Ricochet(pos);
            }
        } else {
            Spawn_Blood(
                hit_pos->x, hit_pos->y, hit_pos->z, item->speed, item->rot.y,
                item->room_num);
        }
    }

    if (item->hit_points > 0) {
        switch (item->object_id) {
        case O_WOLF:
            Sound_Effect(SFX_WOLF_HURT, &item->pos, SPM_NORMAL);
            break;

        case O_BEAR:
            Sound_Effect(SFX_BEAR_HURT, &item->pos, SPM_NORMAL);
            break;

        case O_LION:
        case O_LIONESS:
            Sound_Effect(SFX_LION_HURT, &item->pos, SPM_NORMAL);
            break;

        case O_RAT:
            Sound_Effect(SFX_RAT_CHIRP, &item->pos, SPM_NORMAL);
            break;

        case O_SKATEKID:
            Sound_Effect(SFX_SKATEBOARD_HIT, &item->pos, SPM_NORMAL);
            break;

        case O_TORSO:
            Sound_Effect(SFX_TORSO_HIT, &item->pos, SPM_NORMAL);
            break;

        default:
            break;
        }
    }
}

void Gun_GetNewTarget(const WEAPON_INFO *const weapon)
{
    const ITEM *const lara_item = Lara_GetItem();
    LARA_INFO *const lara = Lara_GetLaraInfo();
    const ITEM *const old_target = lara->target;

    // Preserve OG targeting behavior.
    if (g_Config.gameplay.target_mode == TARGET_LOCK_MODE_FULL
        && !g_Config.gameplay.enable_target_change && !g_Input.action) {
        lara->target = nullptr;
    }

    const GAME_VECTOR start = {
        .x = lara_item->pos.x,
        .y = lara_item->pos.y - 650,
        .z = lara_item->pos.z,
        .room_num = lara_item->room_num,
    };

    M_TARGET_CONTEXT ctx = {
        .weapon = weapon,
        .start = &start,
        .lara_item = lara_item,
        .lara = lara,
        .old_target = old_target,
        .max_dist = weapon->target_dist,
        .best_target = nullptr,
        .best_y_rot = INT16_MAX,
        .best_dist = INT32_MAX,
        .num_targets = 0,
        .old_target_dist = INT32_MAX,
        .old_target_y_rot = INT16_MAX,
        .old_target_in_list = false,
    };

    // First pass: active creatures
    for (int32_t i = 0; i < LOT_SLOT_COUNT; i++) {
        const CREATURE *const creature = LOT_GetBaddieSlot(i);
        if (creature->item_num == NO_ITEM) {
            continue;
        }
        M_ConsiderTarget(&ctx, Item_Get(creature->item_num));
    }

    // Second pass: other objects, including skidoo driver, whose targetable
    // ITEM is NOT in the active item list
    for (int32_t item_num = 0; item_num < Item_GetLevelCount(); item_num++) {
        ITEM *const item = Item_Get(item_num);
        if (M_TargetListContains(item, ctx.num_targets)) {
            continue;
        }
        M_ConsiderTarget(&ctx, item);
    }

    m_TargetCount = ctx.num_targets;

    if ((g_Config.gameplay.target_mode == TARGET_LOCK_MODE_FULL
         || g_Config.gameplay.target_mode == TARGET_LOCK_MODE_SEMI)
        && g_Input.action && lara->target != nullptr) {
        Gun_TargetInfo(weapon);
        return;
    }

    if (ctx.num_targets > 0) {
        bool found_current_target = false;
        for (int16_t slot = 0; slot < ctx.num_targets; slot++) {
            if (m_TargetList[slot] == lara->target) {
                found_current_target = true;
                break;
            }
        }

        if (!found_current_target) {
            lara->target = ctx.best_target;
            m_LastTargetList[0] = nullptr;
        }
    } else {
        lara->target = nullptr;
    }

    if (lara->target != m_LastTargetList[0]) {
        for (int32_t slot = LOT_SLOT_COUNT - 1; slot > 0; slot--) {
            m_LastTargetList[slot] = m_LastTargetList[slot - 1];
        }
        m_LastTargetList[0] = lara->target;
    }

    Gun_TargetInfo(weapon);
}

void Gun_ChangeTarget(const WEAPON_INFO *const weapon)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->target = nullptr;
    bool found_new_target = false;

    for (int16_t new_target = 0; new_target < m_TargetCount; new_target++) {
        for (int32_t last_target = 0; last_target < LOT_SLOT_COUNT;
             last_target++) {
            if (!m_LastTargetList[last_target]) {
                found_new_target = true;
                break;
            }

            if (m_LastTargetList[last_target] == m_TargetList[new_target]) {
                break;
            }
        }

        if (found_new_target) {
            lara->target = m_TargetList[new_target];
            break;
        }
    }

    if (lara->target != m_LastTargetList[0]) {
        for (int32_t last_target = LOT_SLOT_COUNT - 1; last_target > 0;
             last_target--) {
            m_LastTargetList[last_target] = m_LastTargetList[last_target - 1];
        }
        m_LastTargetList[0] = lara->target;
    }

    Gun_TargetInfo(weapon);
}
