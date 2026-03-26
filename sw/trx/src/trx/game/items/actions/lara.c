#include <trx/game/camera.h>
#include <trx/game/gun/smoke.h>
#include <trx/game/lara.h>
#include <trx/game/random.h>
#include <trx/game/rooms.h>
#include <trx/game/sound.h>
#include <trx/game/spawn.h>
#include <trx/game/viewport.h>
#include <trx/version.h>

static void M_Normal(ITEM *const item)
{
    if (item == nullptr) {
        return;
    }

    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->extra_anim = false;
    item->current_anim_state = LS(LS_STOP);
    item->goal_anim_state = LS(LS_STOP);
    Item_SwitchToAnim(item, LA(LA_STAND_STILL), 0);
    g_Camera.type = CAM_CHASE;
    Viewport_AlterFOV(-1, FOV_MODE_GAME);
}

static void M_HandsFree(ITEM *const item)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->gun_status = LGS_ARMLESS;
}

static void M_ToggleGun(
    ITEM *const item, const LARA_MESH thigh_mesh_idx,
    const LARA_MESH hand_mesh_idx)
{
    if (item == nullptr) {
        return;
    }

    const bool armed =
        Lara_Skin_GetEquipment(hand_mesh_idx)->type == EQUIPMENT_TYPE_WEAPON;
    if (armed) {
        Lara_Skin_SetGunEquipment(thigh_mesh_idx, LGT_PISTOLS);
        Lara_Skin_SetGunEquipment(hand_mesh_idx, LGT_UNARMED);
    } else {
        Lara_Skin_SetGunEquipment(thigh_mesh_idx, LGT_UNARMED);
        Lara_Skin_SetGunEquipment(hand_mesh_idx, LGT_PISTOLS);
    }
}

static void M_ToggleRightGun(ITEM *const item)
{
    M_ToggleGun(item, LM_THIGH_R, LM_HAND_R);
}

static void M_ToggleLeftGun(ITEM *const item)
{
    M_ToggleGun(item, LM_THIGH_L, LM_HAND_L);
}

static void M_ShootRightGun(ITEM *const item)
{
    Lara_GetLaraInfo()->right_arm.flash_gun = 3;
    if (g_TRVersion == 3) {
        Spawn_GunShell(LGT_PISTOLS, true);
        Gun_Smoke_OnFire(LGT_PISTOLS, true);
    }
}

static void M_ShootLeftGun(ITEM *const item)
{
    Lara_GetLaraInfo()->left_arm.flash_gun = 3;
    if (g_TRVersion == 3) {
        Spawn_GunShell(LGT_PISTOLS, false);
        Gun_Smoke_OnFire(LGT_PISTOLS, false);
    }
}

static void M_ResetHair(ITEM *const item)
{
    Lara_Hair_Initialise();
}

static void M_Bubbles(ITEM *const item)
{
    // XXX: until we get RoboLara, it makes sense for her to breathe underwater
    const LARA_INFO *const lara = Lara_GetLaraInfo();
    const ITEM *const lara_item = Lara_GetItem();
    if (lara->water_status == LWS_CHEAT
        && !Room_Get(lara_item->room_num)->flags.underwater) {
        return;
    }

    const int32_t count = g_TRVersion == 3 ? (Random_GetControl() & 3) + 2
                                           : (Random_GetDraw() * 3) / 0x8000;
    if (count == 0) {
        return;
    }

    Sound_Effect(SFX_LARA_BUBBLES, &lara_item->pos, SPM_UNDERWATER);

    XYZ_32 offset = { .x = 0, .y = 0, .z = 50 };
    Collide_GetJointAbsPosition(lara_item, &offset, LM_HEAD);

    for (int32_t i = 0; i < count; i++) {
        Spawn_Bubble(&offset, lara_item->room_num);
    }
}

REGISTER_ITEM_ACTION(ITEM_ACTION_LARA_NORMAL, M_Normal)
REGISTER_ITEM_ACTION(ITEM_ACTION_LARA_HANDS_FREE, M_HandsFree)
REGISTER_ITEM_ACTION(ITEM_ACTION_LARA_DRAW_RIGHT_GUN, M_ToggleRightGun)
REGISTER_ITEM_ACTION(ITEM_ACTION_LARA_DRAW_LEFT_GUN, M_ToggleLeftGun)
REGISTER_ITEM_ACTION(ITEM_ACTION_LARA_SHOOT_RIGHT_GUN, M_ShootRightGun)
REGISTER_ITEM_ACTION(ITEM_ACTION_LARA_SHOOT_LEFT_GUN, M_ShootLeftGun)
REGISTER_ITEM_ACTION(ITEM_ACTION_RESET_HAIR, M_ResetHair)
REGISTER_ITEM_ACTION(ITEM_ACTION_BUBBLES, M_Bubbles)
