#include <trx/game/gun/control.h>

#include <trx/config.h>
#include <trx/debug.h>
#include <trx/game/camera.h>
#include <trx/game/collision/los.h>
#include <trx/game/game.h>
#include <trx/game/gun/common.h>
#include <trx/game/gun/misc.h>
#include <trx/game/gun/pistols.h>
#include <trx/game/gun/rifle.h>
#include <trx/game/gun/smashing.h>
#include <trx/game/gun/smoke.h>
#include <trx/game/gun/vars.h>
#include <trx/game/input.h>
#include <trx/game/inventory.h>
#include <trx/game/lara.h>
#include <trx/game/matrix.h>
#include <trx/game/objects/common.h>
#include <trx/game/random.h>
#include <trx/game/rooms.h>
#include <trx/game/sound.h>
#include <trx/game/spawn.h>
#include <trx/game/stats.h>
#include <trx/version.h>

static struct {
    LARA_GUN_TYPE gun_type;
    INPUT_ROLE input_role;
} m_QuicDrawKeys[] = {
    { .gun_type = LGT_PISTOLS, .input_role = INPUT_ROLE_EQUIP_PISTOLS },
    { .gun_type = LGT_SHOTGUN, .input_role = INPUT_ROLE_EQUIP_SHOTGUN },
    { .gun_type = LGT_MAGNUMS, .input_role = INPUT_ROLE_EQUIP_MAGNUMS },
    { .gun_type = LGT_AUTOS, .input_role = INPUT_ROLE_EQUIP_AUTOS },
    { .gun_type = LGT_DESERT_EAGLE,
      .input_role = INPUT_ROLE_EQUIP_DESERT_EAGLE },
    { .gun_type = LGT_UZIS, .input_role = INPUT_ROLE_EQUIP_UZIS },
    { .gun_type = LGT_HARPOON, .input_role = INPUT_ROLE_EQUIP_HARPOON },
    { .gun_type = LGT_M16, .input_role = INPUT_ROLE_EQUIP_M16 },
    { .gun_type = LGT_MP5, .input_role = INPUT_ROLE_EQUIP_MP5 },
    { .gun_type = LGT_GRENADE,
      .input_role = INPUT_ROLE_EQUIP_GRENADE_LAUNCHER },
    { .gun_type = LGT_ROCKET, .input_role = INPUT_ROLE_EQUIP_ROCKET_LAUNCHER },
    { .gun_type = LGT_UNKNOWN, .input_role = (INPUT_ROLE)-1 },
};

static void M_CheckSmashablesBehindTarget(
    const GAME_VECTOR start, const GAME_VECTOR hit_pos, const int32_t max_dist)
{
    // OG does a raycast instead of segment cast when checking for smashables.
    // TRX normally doesn't do that, but in the Reunion battle against Sophia,
    // Sophia stands directly in front of the Fuse Box, preventing Lara from
    // shooting her which is a breaking behavior.
    //
    // This function does additional smashable pass by emulating the OG raycast.
    const int32_t hit_dist = XYZ_32_GetDistance(start.pos, hit_pos.pos);
    if (hit_dist >= max_dist) {
        return;
    }

    const int32_t offset = STEP_L / 16;
    GAME_VECTOR follow_start = {
        .x = hit_pos.x + ((offset * g_MatrixPtr->_20) >> W2V_SHIFT),
        .y = hit_pos.y + ((offset * g_MatrixPtr->_21) >> W2V_SHIFT),
        .z = hit_pos.z + ((offset * g_MatrixPtr->_22) >> W2V_SHIFT),
        .room_num = hit_pos.room_num,
    };
    Room_GetSector(follow_start.pos, &follow_start.room_num);

    GAME_VECTOR follow_end = {
        .x = start.x + ((max_dist * g_MatrixPtr->_20) >> W2V_SHIFT),
        .y = start.y + ((max_dist * g_MatrixPtr->_21) >> W2V_SHIFT),
        .z = start.z + ((max_dist * g_MatrixPtr->_22) >> W2V_SHIFT),
        .room_num = start.room_num,
    };
    Room_GetSector(follow_end.pos, &follow_end.room_num);

    Gun_SmashItems(follow_start, follow_end, nullptr, NO_OBJECT);
}

static bool M_IsUsableUnderwater(const LARA_GUN_TYPE gun_type)
{
    return gun_type == LGT_HARPOON;
}

static bool M_IsTooSubmerged(const LARA_GUN_TYPE gun_type)
{
    const LARA_INFO *const lara = Lara_GetLaraInfo();
    return lara->water_surface_dist > -g_Weapons[gun_type].gun_height;
}

static LARA_GUN_TYPE M_NeedToQuickDraw(void)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    for (int32_t i = 0; m_QuicDrawKeys[i].gun_type != LGT_UNKNOWN; i++) {
        if (Input_IsPressedDB(m_QuicDrawKeys[i].input_role)
            && Inv_RequestItem(Gun_GetGunObject(m_QuicDrawKeys[i].gun_type))
                > 0) {
            return m_QuicDrawKeys[i].gun_type;
        }
    }
    return LGT_UNKNOWN;
}

static bool M_QuickDrawWeapon(void)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    const LARA_GUN_TYPE gun_type = M_NeedToQuickDraw();
    if (gun_type != LGT_UNKNOWN) {
        lara->request_gun_type = gun_type;
        return true;
    }
    return false;
}

static bool M_CanEquip(void)
{
    const LARA_INFO *const lara = Lara_GetLaraInfo();
    if (lara->request_gun_type == LGT_FLARE) {
        return lara->gun_type != LGT_FLARE;
    }
    if (lara->is_crouched && Gun_IsRifleType(lara->request_gun_type)) {
        return false;
    }
    if (Lara_Vehicle_IsMounted()) {
        return false;
    }
    if (!Inv_RequestItem(Gun_GetGunObject(lara->request_gun_type))) {
        return false;
    }
    switch (lara->water_status) {
    case LWS_CHEAT:
        return false;
    case LWS_ABOVE_WATER:
        return true;
    case LWS_SURFACE:
    case LWS_UNDERWATER:
        return M_IsUsableUnderwater(lara->request_gun_type);
    case LWS_WADE: {
        if (M_IsUsableUnderwater(lara->request_gun_type)) {
            return true;
        }
        if (lara->gun_status == LGS_ARMLESS
            || M_IsTooSubmerged(lara->gun_type)) {
            return true;
        }
        return false;
    default:
        ASSERT_FAIL();
        return false;
    }
    }
}

static bool M_HasWeaponAnim(const LARA_GUN_TYPE gun_type)
{
    const OBJECT *const obj = Object_Get(Gun_GetLaraAnim(gun_type));
    return obj->loaded && obj->frame_base != nullptr;
}

static bool M_NeedToDraw(void)
{
    const LARA_INFO *const lara = Lara_GetLaraInfo();
    if (g_Input.draw || lara->request_gun_type != lara->gun_type) {
        return true;
    }
    return false;
}

static bool M_NeedToUndraw(void)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    if (g_Input.draw || lara->request_gun_type != lara->gun_type) {
        return true;
    }
    if (M_QuickDrawWeapon()) {
        if (g_Config.input.quick_guns_mode == QUICK_GUNS_MODE_DRAW_AND_HOLSTER
            || lara->request_gun_type != lara->gun_type) {
            return true;
        }
    }
    switch (lara->water_status) {
    case LWS_CHEAT:
        return true;
    case LWS_UNDERWATER:
    case LWS_SURFACE:
        return !M_IsUsableUnderwater(lara->request_gun_type);
    case LWS_ABOVE_WATER:
        return false;
    case LWS_WADE:
        return !M_IsUsableUnderwater(lara->request_gun_type)
            && !M_IsTooSubmerged(lara->gun_type);
    default:
        return false;
    }
}

static void M_DecideRequestedWeapon(void)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    const ITEM *const lara_item = Lara_GetItem();
    if (g_Input.draw) {
        LARA_GUN_TYPE requested_gun = lara->last_gun_type != LGT_UNARMED
            ? lara->last_gun_type
            : LGT_PISTOLS;
        if (Inv_RequestItem(Gun_GetGunObject(requested_gun)) == 0) {
            for (LARA_GUN_TYPE gun = 0; gun < NUM_WEAPONS; gun++) {
                if (Inv_RequestItem(Gun_GetGunObject(gun)) > 0) {
                    requested_gun = gun;
                    break;
                }
            }
        }
        if (Inv_RequestItem(Gun_GetGunObject(requested_gun)) != 0) {
            lara->request_gun_type = requested_gun;
        }
        return;
    }

    if (g_Input.use_flare) {
        if (lara->gun_type == LGT_FLARE) {
            lara->gun_status = LGS_UNDRAW;
        } else if (
            Inv_RequestItem(O_FLAREBOX_ITEM)
            && (!g_Config.gameplay.fix_free_flare_glitch
                || lara_item->current_anim_state != LS(LS_PICKUP))) {
            lara->request_gun_type = LGT_FLARE;
        }
    }
}

static void M_DrawRequestedWeapon(void)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    if (M_CanEquip()) {
        if (!M_HasWeaponAnim(lara->request_gun_type)) {
            lara->request_gun_type = LGT_UNARMED;
            lara->gun_type = LGT_UNARMED;
            lara->gun_status = LGS_ARMLESS;
            return;
        }

        if (lara->gun_type == LGT_FLARE) {
            Lara_Flare_Dispose(false);
        }

        lara->gun_type = lara->request_gun_type;
        Gun_InitialiseNewWeapon();
        lara->gun_status = LGS_DRAW;
        lara->right_arm.frame_num = 0;
        lara->left_arm.frame_num = 0;
    } else {
        if (lara->request_gun_type != LGT_FLARE
            && lara->request_gun_type != LGT_UNARMED) {
            lara->last_gun_type = lara->request_gun_type;
        }
        if (lara->gun_type == LGT_FLARE) {
            lara->request_gun_type = LGT_FLARE;
        } else {
            lara->gun_type = lara->request_gun_type;
        }
    }
}

static void M_TryUndrawWeapon(void)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    if (g_Input.use_flare && Inv_RequestItem(O_FLAREBOX_ITEM)) {
        lara->request_gun_type = LGT_FLARE;
    }
    if (M_NeedToUndraw()) {
        lara->gun_status = LGS_UNDRAW;
    }
}

static void M_UpdateGunState(void)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    const ITEM *const lara_item = Lara_GetItem();
    if (lara_item->hit_points <= 0) {
        lara->gun_status = LGS_ARMLESS;
    } else if (lara->gun_status == LGS_ARMLESS) {
        if (M_QuickDrawWeapon()) {
            M_DrawRequestedWeapon();
        } else {
            M_DecideRequestedWeapon();
            if (M_NeedToDraw()) {
                M_DrawRequestedWeapon();
            }
        }
    } else if (lara->gun_status == LGS_READY) {
        M_TryUndrawWeapon();
    } else {
        M_QuickDrawWeapon();
    }
}

void Gun_Control(void)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    const ITEM *const lara_item = Lara_GetItem();

    if (lara->extra_anim && lara->gun_status != LGS_HANDS_BUSY) {
        lara->request_gun_type = LGT_UNARMED;
        return;
    }

    if (lara->left_arm.flash_gun > 0) {
        lara->left_arm.flash_gun--;
    }
    if (lara->right_arm.flash_gun > 0) {
        lara->right_arm.flash_gun--;
    }

    Gun_Smoke_Control();

    M_UpdateGunState();

    switch (lara->gun_status) {
    case LGS_ARMLESS:
    case LGS_HANDS_BUSY:
        if (lara->gun_type == LGT_FLARE) {
            Lara_Flare_Control();
        }
        break;

    case LGS_DRAW:
        if (lara->gun_type != LGT_FLARE && lara->gun_type != LGT_UNARMED) {
            lara->last_gun_type = lara->gun_type;
        }

        switch (lara->gun_type) {
        case LGT_PISTOLS:
        case LGT_MAGNUMS:
        case LGT_AUTOS:
        case LGT_DESERT_EAGLE:
        case LGT_UZIS:
            if (g_Camera.type != CAM_CINEMATIC && g_Camera.type != CAM_LOOK) {
                g_Camera.type = CAM_COMBAT;
            }
            Gun_Pistols_Draw(lara->gun_type);
            break;

        case LGT_SHOTGUN:
        case LGT_M16:
        case LGT_MP5:
        case LGT_GRENADE:
        case LGT_ROCKET:
        case LGT_HARPOON:
            if (g_Camera.type != CAM_CINEMATIC && g_Camera.type != CAM_LOOK) {
                g_Camera.type = CAM_COMBAT;
            }
            Gun_Rifle_Draw(lara->gun_type);
            break;

        case LGT_FLARE:
            Lara_Flare_Draw();
            break;

        default:
            lara->gun_status = LGS_ARMLESS;
            break;
        }
        break;

    case LGS_UNDRAW:
        Lara_Skin_SetCombatFace(false);

        switch (lara->gun_type) {
        case LGT_PISTOLS:
        case LGT_MAGNUMS:
        case LGT_AUTOS:
        case LGT_DESERT_EAGLE:
        case LGT_UZIS:
            Gun_Pistols_Undraw(lara->gun_type);
            break;

        case LGT_SHOTGUN:
        case LGT_M16:
        case LGT_MP5:
        case LGT_GRENADE:
        case LGT_ROCKET:
        case LGT_HARPOON:
            Gun_Rifle_Undraw(lara->gun_type);
            break;

        case LGT_FLARE:
            Lara_Flare_Undraw();
            break;

        default:
            return;
        }
        break;

    case LGS_READY:
        const bool is_firing = lara->pistol_ammo.ammo != 0 && g_Input.action;
        Lara_Skin_SetCombatFace(is_firing);

        if (g_Camera.type != CAM_CINEMATIC && g_Camera.type != CAM_LOOK) {
            g_Camera.type = CAM_COMBAT;
        }

        if (g_Input.action) {
            AMMO_INFO *const ammo = Gun_GetAmmoInfo(lara->gun_type);
            ASSERT(ammo != nullptr);

            if (ammo->ammo <= 0) {
                ammo->ammo = 0;
                if (g_TRVersion >= 2) {
                    Sound_Effect(SFX_CLICK, &lara_item->pos, SPM_NORMAL);
                }
                lara->request_gun_type =
                    Inv_RequestItem(O_PISTOL_ITEM) ? LGT_PISTOLS : LGT_UNARMED;
                break;
            }
        }

        switch (lara->gun_type) {
        case LGT_PISTOLS:
        case LGT_MAGNUMS:
        case LGT_AUTOS:
        case LGT_DESERT_EAGLE:
        case LGT_UZIS:
            Gun_Pistols_Control(lara->gun_type);
            break;

        case LGT_SHOTGUN:
        case LGT_M16:
        case LGT_MP5:
        case LGT_GRENADE:
        case LGT_ROCKET:
        case LGT_HARPOON:
            Gun_Rifle_Control(lara->gun_type);
            break;

        default:
            return;
        }
        break;

    case LGS_SPECIAL:
        Lara_Flare_Draw();
        break;

    default:
        return;
    }
}

void Gun_EnsureReady(void)
{
    const LARA_INFO *const lara = Lara_GetLaraInfo();
    if (lara->gun_status == LGS_READY && Gun_IsRifleType(lara->gun_type)) {
        Gun_Rifle_EnsureReady(lara->gun_type);
    }
}

int32_t Gun_FireWeapon(
    const LARA_GUN_TYPE weapon_type, ITEM *const target, const ITEM *const src,
    const int16_t *const angles)
{
    const WEAPON_INFO *const weapon = &g_Weapons[weapon_type];
    LARA_INFO *const lara = Lara_GetLaraInfo();

    AMMO_INFO *const ammo = Gun_GetAmmoInfo(weapon_type);
    ASSERT(ammo != nullptr);

    if (ammo == &lara->pistol_ammo || Game_IsBonusFlagSet(GBF_NGPLUS)) {
        ammo->ammo = 1000;
    }
    if (ammo->ammo <= 0) {
        ammo->ammo = 0;
        if (g_TRVersion == 1) {
            Sound_Effect(SFX_LARA_EMPTY, &src->pos, SPM_NORMAL);
            if (Inv_RequestItem(O_PISTOL_ITEM)) {
                lara->request_gun_type = LGT_PISTOLS;
            } else {
                lara->gun_status = LGS_UNDRAW;
            }
        }
        return 0;
    }
    ammo->ammo--;
    Stats_AddAmmoUsed();
    lara->has_fired = true;

    const XYZ_32 view_pos = {
        .x = src->pos.x,
        .y = src->pos.y - weapon->gun_height,
        .z = src->pos.z,
    };
    const XYZ_16 view_rot = {
        .x = angles[1]
            + weapon->shot_accuracy * (Random_GetControl() - DEG_90) / DEG_360,
        .y = angles[0]
            + weapon->shot_accuracy * (Random_GetControl() - DEG_90) / DEG_360,
        .z = 0,
    };
    Matrix_GenerateW2V(&view_pos, &view_rot);

    SPHERE spheres[33];
    int32_t sphere_count = Collide_GetSpheres(target, spheres, false);
    int32_t best_sphere = -1;
    int32_t best_dist = INT32_MAX;

    for (int32_t i = 0; i < sphere_count; i++) {
        const SPHERE *const sphere = &spheres[i];
        const int32_t r = sphere->r;
        if (ABS(sphere->pos.x) < r && ABS(sphere->pos.y) < r
            && sphere->pos.z > r
            && SQUARE(sphere->pos.x) + SQUARE(sphere->pos.y) <= SQUARE(r)) {
            const int32_t dist = sphere->pos.z - r;
            if (dist < best_dist) {
                best_dist = dist;
                best_sphere = i;
            }
        }
    }

    GAME_VECTOR start = {
        .pos = view_pos,
        .room_num = src->room_num,
    };

    if (best_sphere < 0) {
        const int32_t dist = weapon->target_dist;
        GAME_VECTOR hit_pos = g_TRVersion == 1
            ? (GAME_VECTOR) {
                .x = start.x + g_MatrixPtr->_20,
                .y = start.y + g_MatrixPtr->_21,
                .z = start.z + g_MatrixPtr->_22,
                .room_num = start.room_num,
            }
            : (GAME_VECTOR) {
                .x = start.x + ((dist * g_MatrixPtr->_20) >> W2V_SHIFT),
                .y = start.y + ((dist * g_MatrixPtr->_21) >> W2V_SHIFT),
                .z = start.z + ((dist * g_MatrixPtr->_22) >> W2V_SHIFT),
                .room_num = start.room_num,
            };
        Room_GetSector(hit_pos.pos, &hit_pos.room_num);
        const bool object_on_los = LOS_Check(&start, &hit_pos, true);
        if (Gun_SmashItems(start, hit_pos, &hit_pos.pos, NO_OBJECT)
            == PROJECTILE_HIT_STOP) {
            Room_GetSector(hit_pos.pos, &hit_pos.room_num);
        }
        if (!object_on_los) {
            Spawn_RicochetRay(start, hit_pos);
        }
        M_CheckSmashablesBehindTarget(start, hit_pos, weapon->target_dist);
        return -1;
    }

    Stats_AddAmmoHits();
    GAME_VECTOR hit_pos = {
        .x = start.x + ((best_dist * g_MatrixPtr->_20) >> W2V_SHIFT),
        .y = start.y + ((best_dist * g_MatrixPtr->_21) >> W2V_SHIFT),
        .z = start.z + ((best_dist * g_MatrixPtr->_22) >> W2V_SHIFT),
        .room_num = start.room_num,
    };
    Room_GetSector(hit_pos.pos, &hit_pos.room_num);
    Gun_SmashItems(start, hit_pos, nullptr, NO_OBJECT);
    Gun_HitTarget(
        target, &start, &hit_pos,
        weapon->damage * (Game_IsBonusFlagSet(GBF_JAPANESE) ? 2 : 1));
    M_CheckSmashablesBehindTarget(start, hit_pos, weapon->target_dist);
    return 1;
}
