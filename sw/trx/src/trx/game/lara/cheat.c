#include <trx/game/lara/cheat.h>

#include <trx/core/vector.h>
#include <trx/game/camera.h>
#include <trx/game/console.h>
#include <trx/game/game.h>
#include <trx/game/game_strings/entries.h>
#include <trx/game/gun.h>
#include <trx/game/input.h>
#include <trx/game/interpolation.h>
#include <trx/game/inventory.h>
#include <trx/game/items.h>
#include <trx/game/lara.h>
#include <trx/game/objects.h>
#include <trx/game/rooms.h>
#include <trx/game/sound.h>
#include <trx/game/viewport.h>
#include <trx/version.h>

static void M_GiveAllKeysImpl(void)
{
    Inv_AddItem(O_PUZZLE_ITEM_1);
    Inv_AddItem(O_PUZZLE_ITEM_2);
    Inv_AddItem(O_PUZZLE_ITEM_3);
    Inv_AddItem(O_PUZZLE_ITEM_4);
    Inv_AddItem(O_KEY_ITEM_1);
    Inv_AddItem(O_KEY_ITEM_2);
    Inv_AddItem(O_KEY_ITEM_3);
    Inv_AddItem(O_KEY_ITEM_4);
    Inv_AddItem(O_QUEST_ITEM_1);
    Inv_AddItem(O_QUEST_ITEM_2);
    Inv_AddItem(O_QUEST_ITEM_3);
    Inv_AddItem(O_QUEST_ITEM_4);
    Inv_AddItem(O_PICKUP_ITEM_1);
    Inv_AddItem(O_PICKUP_ITEM_2);
    Inv_AddItem(O_LEADBAR_ITEM);
}

static void M_GiveAllGunsImpl(const bool ignore_exclusions)
{
    LARA_INFO *const lara_info = Lara_GetLaraInfo();
    const bool bonus_flag = Game_IsBonusFlagSet(GBF_NGPLUS);
    Inv_AddItem(O_PISTOL_ITEM);
    if (Lara_Cheat_GiveGun(LGT_SHOTGUN, ignore_exclusions)) {
        lara_info->shotgun_ammo.ammo = bonus_flag ? 10001 : 300;
    }
    if (Lara_Cheat_GiveGun(LGT_MAGNUMS, ignore_exclusions)) {
        lara_info->magnum_ammo.ammo = bonus_flag ? 10001 : 1000;
    }
    if (Lara_Cheat_GiveGun(LGT_AUTOS, ignore_exclusions)) {
        lara_info->autos_ammo.ammo = bonus_flag ? 10001 : 1000;
    }
    if (Lara_Cheat_GiveGun(LGT_DESERT_EAGLE, ignore_exclusions)) {
        lara_info->desert_eagle_ammo.ammo = bonus_flag ? 10001 : 1000;
    }
    if (Lara_Cheat_GiveGun(LGT_UZIS, ignore_exclusions)) {
        lara_info->uzi_ammo.ammo = bonus_flag ? 10001 : 2000;
    }
    if (Lara_Cheat_GiveGun(LGT_HARPOON, ignore_exclusions)) {
        lara_info->harpoon_ammo.ammo = bonus_flag ? 10001 : 300;
    }
    if (Lara_Cheat_GiveGun(LGT_M16, ignore_exclusions)) {
        lara_info->m16_ammo.ammo = bonus_flag ? 10001 : 300;
    }
    if (Lara_Cheat_GiveGun(LGT_MP5, ignore_exclusions)) {
        lara_info->mp5_ammo.ammo = bonus_flag ? 10001 : 300;
    }
    if (Lara_Cheat_GiveGun(LGT_GRENADE, ignore_exclusions)) {
        lara_info->grenade_ammo.ammo = bonus_flag ? 10001 : 300;
    }
    if (Lara_Cheat_GiveGun(LGT_ROCKET, ignore_exclusions)) {
        lara_info->rocket_ammo.ammo = bonus_flag ? 10001 : 300;
    }
}

static void M_GiveAllMedpacksImpl(void)
{
    if (g_Weapons[LGT_FLARE].is_available) {
        Inv_AddItemNTimes(O_FLAREBOX_ITEM, 10);
    }
    Inv_AddItemNTimes(O_SMALL_MEDIPACK_ITEM, 10);
    Inv_AddItemNTimes(O_LARGE_MEDIPACK_ITEM, 10);
}

static void M_ReinitialiseGunMeshes(void)
{
    Lara_Mesh_Initialise(Game_GetCurrentLevel());
    Gun_InitialiseNewWeapon();
}

static void M_ClearHandWeaponMeshes(void)
{
    Gun_SetLaraHandRMesh(LGT_UNARMED);
    if (!Lara_Flare_IsMeshActive()) {
        Gun_SetLaraHandLMesh(LGT_UNARMED);
    }
}

static void M_ResetGunStatus(void)
{
    const ITEM *const lara_item = Lara_GetItem();
    LARA_INFO *const lara_info = Lara_GetLaraInfo();
    const bool has_flare = Lara_Flare_IsMeshActive();
    if (has_flare) {
        lara_info->gun_type = LGT_FLARE;
        return;
    }

    lara_info->gun_status = LGS_ARMLESS;
    lara_info->gun_type = LGT_UNARMED;
    lara_info->request_gun_type = LGT_UNARMED;
    lara_info->gun_item_num = NO_ITEM;
    lara_info->left_arm.frame_num = 0;
    lara_info->left_arm.lock = 0;
    lara_info->right_arm.frame_num = 0;
    lara_info->right_arm.lock = 0;
    lara_info->left_arm.anim_num = lara_item->anim_num;
    lara_info->right_arm.anim_num = lara_item->anim_num;

    const ANIM *const anim = Item_GetAnim(lara_item);
    lara_info->left_arm.frame_base = anim->frame_ptr;
    lara_info->right_arm.frame_base = anim->frame_ptr;
}

bool Lara_Cheat_GiveAllKeys(void)
{
    if (Lara_GetItem() == nullptr) {
        return false;
    }

    M_GiveAllKeysImpl();

    Sound_Effect(SFX_LARA_KEY, nullptr, SPM_ALWAYS);
    Console_Log(GS("general/osd/give_item_all_keys"));
    return true;
}

bool Lara_Cheat_GiveAllGuns(const bool ignore_exclusions)
{
    if (Lara_GetItem() == nullptr) {
        return false;
    }

    M_GiveAllGunsImpl(ignore_exclusions);

    Sound_Effect(SFX_LARA_RELOAD, nullptr, SPM_ALWAYS);
    Console_Log(GS("general/osd/give_item_all_guns"));
    return true;
}

bool Lara_Cheat_GiveGun(
    const LARA_GUN_TYPE gun_type, const bool ignore_exclusions)
{
    const OBJECT_ID gun_object_id = Gun_GetGunObject(gun_type);
    if (gun_object_id == NO_OBJECT) {
        return false;
    }

    if (!ignore_exclusions && !g_Weapons[gun_type].is_available) {
        return false;
    }

    return Inv_AddItem(gun_object_id);
}

bool Lara_Cheat_GiveAllItems(void)
{
    if (Lara_GetItem() == nullptr) {
        return false;
    }

    M_GiveAllGunsImpl(false);
    M_GiveAllKeysImpl();
    M_GiveAllMedpacksImpl();

    Sound_Effect(SFX_LARA_HOLSTER, nullptr, SPM_NORMAL);
    Console_Log(GS("general/osd/give_item_cheat"));
    return true;
}

void Lara_Cheat_GetStuff(void)
{
    M_GiveAllGunsImpl(false);
    M_GiveAllMedpacksImpl();
}

void Lara_Cheat_EndLevel(void)
{
    Game_SetIsLevelComplete(true);
    Console_Log(GS("general/osd/complete_level"));
}

bool Lara_Cheat_KillEnemy(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    if ((item->flags & IF_KILLED) != 0) {
        return false;
    }
    if (!Item_IsAlive(item) && item->status != IS_ACTIVE) {
        return false;
    }

    if (Object_IsType(item->object_id, g_LoyalObjects)) {
        LARA_INFO *const lara_info = Lara_GetLaraInfo();
        lara_info->killed_loyal_item = true;
    }

    Sound_Effect(SFX_EXPLOSION_1, &item->pos, SPM_NORMAL);
    Creature_Die(item_num, true);
    return true;
}

bool Lara_Cheat_OpenNearestDoor(void)
{
    const ITEM *const lara_item = Lara_GetItem();
    if (lara_item == nullptr) {
        return false;
    }

    int32_t opened = 0;
    int32_t closed = 0;

    const int32_t shift = 8; // constant shift to avoid overflow errors
    const int32_t max_dist = SQUARE((WALL_L * 2) >> shift);
    for (int32_t item_num = 0; item_num < Item_GetLevelCount(); item_num++) {
        ITEM *const item = Item_Get(item_num);
        if (!Object_IsType(item->object_id, g_DoorObjects)
            && !Object_IsType(item->object_id, g_TrapdoorObjects)) {
            continue;
        }

        const int32_t dx = (item->pos.x - lara_item->pos.x) >> shift;
        const int32_t dy = (item->pos.y - lara_item->pos.y) >> shift;
        const int32_t dz = (item->pos.z - lara_item->pos.z) >> shift;
        const int32_t dist = SQUARE(dx) + SQUARE(dy) + SQUARE(dz);
        if (dist > max_dist) {
            continue;
        }

        if (!item->active) {
            Item_AddActive(item_num);
            item->flags |= IF_CODE_BITS;
            opened++;
        } else if ((item->flags & IF_CODE_BITS) != 0) {
            item->flags &= ~IF_CODE_BITS;
            closed++;
        } else {
            item->flags |= IF_CODE_BITS;
            opened++;
        }
        item->timer = 0;
        item->touch_bits = 0;
    }

    if (opened > 0 || closed > 0) {
        Console_Log(
            opened > 0 ? GS("general/osd/door_open")
                       : GS("general/osd/door_close"));
        return true;
    }
    Console_LogError(GS("general/osd/door_open_fail"));
    return false;
}

bool Lara_Cheat_EnterFlyMode(void)
{
    ITEM *const lara_item = Lara_GetItem();
    LARA_INFO *const lara_info = Lara_GetLaraInfo();
    if (lara_item == nullptr) {
        return false;
    }

    if ((lara_item->flags & IF_ONE_SHOT) != 0) {
        // The explosion cheat has been used, so Lara's death is permanent.
        return false;
    }

    Viewport_AlterFOV(-1, FOV_MODE_GAME);

    if (lara_info->extra_anim || lara_item->hit_points < 0) {
        M_ResetGunStatus();
        M_ClearHandWeaponMeshes();
    } else if (Gun_IsRifleType(lara_info->gun_type)) {
        while (lara_info->gun_item_num != NO_ITEM) {
            Gun_Rifle_Undraw(lara_info->gun_type);
        }
    }

    if (lara_info->gun_status == LGS_HANDS_BUSY
        || (lara_info->gun_status == LGS_UNDRAW
            && Lara_Skin_GetEquipment(LM_TORSO)->type
                == EQUIPMENT_TYPE_WEAPON)) {
        lara_info->gun_status = LGS_ARMLESS;
        M_ClearHandWeaponMeshes();
    }

    lara_info->extra_anim = false;
    Lara_Vehicle_Dismount();
    if (lara_info->water_status != LWS_UNDERWATER
        || lara_item->hit_points <= 0) {
        lara_item->pos.y -= STEP_L;
        lara_item->current_anim_state = LS(LS_SWIM);
        lara_item->goal_anim_state = LS(LS_SWIM);
        Item_SwitchToAnim(lara_item, LA(LA_UNDERWATER_SWIM_FORWARD_DRIFT), 0);
        lara_item->gravity = false;
        lara_item->rot.x = 30 * DEG_1;
        lara_item->fall_speed = 30;
        lara_info->head_rot.x = 0;
        lara_info->head_rot.y = 0;
        lara_info->torso_rot.x = 0;
        lara_info->torso_rot.y = 0;
    }
    lara_info->water_status = LWS_CHEAT;
    lara_info->hit_effect_count = 0;
    lara_info->hit_effect = nullptr;
    lara_info->hit_frame = 0;
    lara_info->hit_direction = -1;
    lara_info->air = LARA_MAX_AIR;
    lara_info->death_timer = 0;
    lara_info->mesh_effects = 0;
    lara_item->enable_shadow = true;
    lara_item->hit_points = LARA_MAX_HITPOINTS;
    lara_info->interact_target.item_num = NO_ITEM;
    lara_info->interact_target.is_moving = false;
    lara_info->interact_target.move_count = 0;

    Lara_Extinguish();
    M_ReinitialiseGunMeshes();
    Lara_Skin_ApplyOutfit();
    g_Camera.type = CAM_CHASE;

    Console_Log(GS("general/osd/fly_mode_on"));
    return true;
}

bool Lara_Cheat_ExitFlyMode(void)
{
    ITEM *const lara_item = Lara_GetItem();
    LARA_INFO *const lara_info = Lara_GetLaraInfo();
    if (lara_item == nullptr) {
        return false;
    }

    const ROOM *const room = Room_Get(lara_item->room_num);
    const int16_t water_height =
        Room_GetWaterHeight(lara_item->pos, lara_item->room_num);

    if (room->flags.underwater
        || (water_height != NO_HEIGHT && water_height > 0
            && !room->flags.swamp)) {
        lara_info->water_status = LWS_UNDERWATER;
    } else {
        lara_info->water_status =
            room->flags.swamp ? LWS_WADE : LWS_ABOVE_WATER;
        Item_SwitchToAnim(lara_item, LA(LA_STAND_STILL), 0);
        lara_item->goal_anim_state = LS(LS_STOP);
        lara_item->current_anim_state = LS(LS_STOP);
        lara_item->rot.x = 0;
        lara_item->rot.z = 0;
        lara_info->head_rot.x = 0;
        lara_info->head_rot.y = 0;
        lara_info->torso_rot.x = 0;
        lara_info->torso_rot.y = 0;
    }

    if (lara_info->gun_item_num != NO_ITEM) {
        lara_info->gun_status = LGS_UNDRAW;
    } else {
        lara_info->gun_status = LGS_ARMLESS;
        M_ClearHandWeaponMeshes();
        M_ReinitialiseGunMeshes();
    }

    if (lara_info->water_status == LWS_ABOVE_WATER) {
        // Prevent Lara from jumping if the player holds the swim button
        // during the fly cheat exit (#4470)
        InputState_Clear(&g_Input);
        InputState_Clear(&g_InputDB);
        Lara_Control();
    }

    Console_Log(GS("general/osd/fly_mode_off"));
    return true;
}

bool Lara_Cheat_Teleport(XYZ_32 pos, int16_t room_num)
{
    if (!Room_FindValidPos(&pos, &room_num)) {
        return false;
    }

    const SECTOR *const sector = Room_GetSector(pos, &room_num);
    const int32_t height = Room_GetHeightEx(sector, pos, true, NO_ITEM);
    if (height == NO_HEIGHT) {
        return false;
    }

    ITEM *const lara_item = Lara_GetItem();
    lara_item->pos.x = pos.x;
    lara_item->pos.y = pos.y;
    lara_item->pos.z = pos.z;
    lara_item->floor = height;

    const int16_t item_num = Item_GetIndex(lara_item);
    Item_UpdateRoom(item_num, room_num);

    LARA_INFO *const lara_info = Lara_GetLaraInfo();
    if (lara_info->gun_status == LGS_HANDS_BUSY) {
        lara_info->gun_status = LGS_ARMLESS;
    }

    Lara_Vehicle_Dismount();
    if (lara_info->extra_anim) {
        const ROOM *const room = Room_Get(lara_item->room_num);
        const bool room_submerged = room->flags.underwater;
        const int16_t water_height =
            Room_GetWaterHeight(lara_item->pos, lara_item->room_num);

        if (room_submerged || (water_height != NO_HEIGHT && water_height > 0)) {
            lara_info->water_status = LWS_UNDERWATER;
            lara_item->current_anim_state = LS(LS_SWIM);
            lara_item->goal_anim_state = LS(LS_SWIM);
            Item_SwitchToAnim(
                lara_item, LA(LA_UNDERWATER_SWIM_FORWARD_DRIFT), 0);
        } else {
            lara_info->water_status = LWS_ABOVE_WATER;
            lara_item->current_anim_state = LS(LS_STOP);
            lara_item->goal_anim_state = LS(LS_STOP);
            Item_SwitchToAnim(lara_item, LA(LA_STAND_STILL), 0);
            lara_item->rot.x = 0;
            lara_item->rot.z = 0;
            lara_info->head_rot.x = 0;
            lara_info->head_rot.y = 0;
            lara_info->torso_rot.x = 0;
            lara_info->torso_rot.y = 0;
        }

        lara_info->extra_anim = false;
        M_ResetGunStatus();
        M_ReinitialiseGunMeshes();
    }

    lara_info->hit_effect_count = 0;
    lara_info->hit_effect = nullptr;
    lara_info->hit_frame = 0;
    lara_info->hit_direction = -1;
    lara_info->air = LARA_MAX_AIR;
    lara_info->death_timer = 0;
    lara_info->mesh_effects = 0;

    if (g_Camera.type == CAM_PHOTO_MODE) {
        Lara_Hair_Control(false);
        Interpolation_CommitLara();
    } else {
        g_Camera.type = CAM_CHASE;
        Viewport_AlterFOV(-1, FOV_MODE_GAME);
        Camera_ResetPosition();
    }

    return true;
}
