#include <trx/game/lara/flare.h>

#include <trx/config.h>
#include <trx/game/game.h>
#include <trx/game/gun.h>
#include <trx/game/input.h>
#include <trx/game/inventory.h>
#include <trx/game/lara.h>
#include <trx/game/objects/general/flare_item.h>
#include <trx/game/random.h>
#include <trx/game/rooms.h>
#include <trx/game/sound.h>
#include <trx/game/sparks.h>
#include <trx/version.h>

typedef enum {
    // clang-format off
    LA_FLARES_HOLD   = 0,
    LA_FLARES_THROW  = 1,
    LA_FLARES_DRAW   = 2,
    LA_FLARES_IGNITE = 3,
    LA_FLARES_IDLE   = 4,
    // clang-format on
} M_LARA_FLARE_ANIMATION;

typedef enum {
    // clang-format off
    LF_FL_HOLD_FT       = 1,
    LF_FL_THROW_FT      = 32,
    LF_FL_DRAW_FT       = 39,
    LF_FL_IGNITE_FT     = 23,
    LF_FL_2_HOLD_FT     = 15,

    LF_FL_HOLD          = 0,
    LF_FL_THROW         = (LF_FL_HOLD + LF_FL_HOLD_FT), // = 1
    LF_FL_THROW_RELEASE = (LF_FL_THROW + 20), // = 21
    LF_FL_DRAW          = (LF_FL_THROW + LF_FL_THROW_FT), // = 33
    LF_FL_IGNITE        = (LF_FL_DRAW + LF_FL_DRAW_FT), // = 72
    LF_FL_2_HOLD        = (LF_FL_IGNITE + LF_FL_IGNITE_FT), // = 95
    LF_FL_END           = (LF_FL_2_HOLD + LF_FL_2_HOLD_FT), // = 110
    LF_FL_DRAW_GOT_IT   = (LF_FL_DRAW + 13), // = 46
    // clang-format on
} M_LARA_FLARE_FRAME;

static const LARA_TRX_STATE m_HoldStates[] = {
    // clang-format off
    LS_WALK,
    LS_STOP,
    LS_POSE,
    LS_TURN_RIGHT,
    LS_TURN_LEFT,
    LS_WALK_BACK,
    LS_FAST_TURN,
    LS_STEP_LEFT,
    LS_STEP_RIGHT,
    LS_WADE,
    LS_PICKUP,
    LS_SWITCH_ON,
    LS_SWITCH_OFF,
    LS_TRX_INVALID, // sentinel
    // clang-format on
};

static const LARA_TRX_STATE m_ThrowStates[] = {
    // clang-format off
    LS_FAST_FALL,
    LS_SWAN_DIVE,
    LS_FAST_DIVE,
    LS_TRX_INVALID, // sentinel
    // clang-format on
};

static XYZ_32 m_IgnitePos = {};

static void M_InitialiseState(void)
{
    LARA_INFO *const lara_info = Lara_GetLaraInfo();
    lara_info->gun_status = LGS_ARMLESS;
    lara_info->left_arm.rot.x = 0;
    lara_info->left_arm.rot.y = 0;
    lara_info->left_arm.rot.z = 0;
    lara_info->right_arm.rot.x = 0;
    lara_info->right_arm.rot.y = 0;
    lara_info->right_arm.rot.z = 0;
    lara_info->left_arm.lock = 0;
    lara_info->right_arm.lock = 0;
    lara_info->target = nullptr;
}

static void M_DoIgniteEffects(const XYZ_32 flare_pos, int16_t room_num)
{
    m_IgnitePos = flare_pos;
    Room_GetSector(m_IgnitePos, &room_num);
    const ROOM *const room = Room_Get(room_num);
    const SOUND_PLAY_MODE mode =
        room->flags.underwater ? SPM_UNDERWATER : SPM_NORMAL;
    Sound_Effect(SFX_LARA_FLARE_IGNITE, &m_IgnitePos, mode);
}

static bool M_CanThrowFlare(void)
{
    const ITEM *const lara_item = Lara_GetItem();
    const LARA_INFO *const lara_info = Lara_GetLaraInfo();
    if (lara_info->gun_status != LGS_ARMLESS) {
        return false;
    }

    if (!g_Config.gameplay.fix_flare_throw_priority) {
        return true;
    }

    if (lara_info->water_status != LWS_ABOVE_WATER
        && lara_info->water_status != LWS_WADE) {
        return true;
    }

    // Airborne states that would not allow ledge grabbing anyway.
    if (Lara_HasState(m_ThrowStates)) {
        return true;
    }

    // Neither airborne nor about to be.
    return !lara_item->gravity && !g_Input.jump;
}

static void M_ControlInHand(void)
{
    LARA_INFO *const lara_info = Lara_GetLaraInfo();
    const int32_t flare_age = g_Config.debug.enable_endless_flare_time
        ? MIN(Flare_GetMaxAge() / 2, lara_info->flare.age)
        : lara_info->flare.age;

    XYZ_32 vec = {
        .x = 11,
        .y = 32,
        .z = 41,
    };
    Lara_GetJointAbsPosition(&vec, LM_HAND_L);

    const ITEM *const lara_item = Lara_GetItem();
    if (flare_age == 0) {
        M_DoIgniteEffects(vec, lara_item->room_num);
    }

    lara_info->left_arm.flash_gun = Flare_GenerateLight(vec, flare_age);

    if (flare_age >= Flare_GetMaxAge()) {
        if (M_CanThrowFlare()) {
            lara_info->gun_status = LGS_UNDRAW;
        }
        return;
    }

    lara_info->flare.age = flare_age + 1;
    Flare_GenerateEffects(&lara_item->pos, vec, lara_item->room_num);

    if (!lara_info->left_arm.flash_gun) {
        return;
    }

    if (g_TRVersion < 3) {
        return;
    }

    XYZ_32 vec_2 = {
        .x = 8,
        .y = 36,
        .z = WALL_L + (Random_GetDraw() & 0xFF),
    };
    Lara_GetJointAbsPosition(&vec_2, LM_HAND_L);
    const XYZ_32 vel = {
        .x = vec_2.x - vec.x,
        .y = vec_2.y - vec.y,
        .z = vec_2.z - vec.z,
    };
    for (int32_t i = 0; i < (Random_GetDraw() & 3) + 4; i++) {
        const bool smoke = (i >> 2) != 0;
        Sparks_TriggerFlareSparks(vec, vel, smoke);
    }
}

static void M_SetArm(const int32_t flare_frame)
{
    int16_t anim_idx;
    if (flare_frame < LF_FL_THROW) {
        anim_idx = LA_FLARES_HOLD;
    } else if (flare_frame < LF_FL_DRAW) {
        anim_idx = LA_FLARES_THROW;
    } else if (flare_frame < LF_FL_IGNITE) {
        anim_idx = LA_FLARES_DRAW;
    } else if (flare_frame < LF_FL_2_HOLD) {
        anim_idx = LA_FLARES_IGNITE;
    } else {
        anim_idx = LA_FLARES_IDLE;
    }

    const OBJECT *const obj = Object_Get(O_LARA_FLARE);
    const ANIM *const anim = Object_GetAnim(obj, anim_idx);
    LARA_INFO *const lara_info = Lara_GetLaraInfo();
    lara_info->left_arm.anim_num = obj->anim_idx + anim_idx;
    lara_info->left_arm.frame_base = anim->frame_ptr;
}

static bool M_CanUseFlareControl(void)
{
    const ITEM *const lara_item = Lara_GetItem();
    if (lara_item->current_anim_state == LS(LS_PICKUP)) {
        const LARA_TRX_ANIMATION anim = LA_U(Item_GetRelativeAnim(lara_item));
        return anim != LA_CROUCH_PICKUP && anim != LA_CRAWL_PICKUP;
    }
    return Lara_Vehicle_IsMounted() || Lara_HasState(m_HoldStates);
}

static void M_ControlArmless(void)
{
    LARA_INFO *const lara_info = Lara_GetLaraInfo();
    if (M_CanUseFlareControl()) {
        if (!lara_info->flare.control) {
            lara_info->left_arm.frame_num = LF_FL_2_HOLD;
            lara_info->flare.control = true;
        } else if (lara_info->left_arm.frame_num != LF_FL_HOLD) {
            lara_info->left_arm.frame_num++;
            if (lara_info->left_arm.frame_num == LF_FL_END) {
                lara_info->left_arm.frame_num = LF_FL_HOLD;
            }
        }
    } else {
        lara_info->flare.control = false;
    }

    M_ControlInHand();
    M_SetArm(lara_info->left_arm.frame_num);
}

static void M_ControlBusyHands(void)
{
    const ITEM *const lara_item = Lara_GetItem();
    LARA_INFO *const lara_info = Lara_GetLaraInfo();
    lara_info->flare.control = M_CanUseFlareControl();
    M_ControlInHand();
    M_SetArm(lara_info->left_arm.frame_num);
}

static void M_UndrawMeshes(void)
{
    Lara_Skin_ClearEquipment(LM_HAND_L);
}

void Lara_Flare_Control(void)
{
    LARA_INFO *const lara_info = Lara_GetLaraInfo();
    if (lara_info->gun_status == LGS_ARMLESS) {
        M_ControlArmless();
    } else if (lara_info->gun_status == LGS_HANDS_BUSY) {
        M_ControlBusyHands();
    }
}

void Lara_Flare_Draw(void)
{
    const ITEM *const lara_item = Lara_GetItem();
    LARA_INFO *const lara_info = Lara_GetLaraInfo();

    if (lara_item->current_anim_state == LS(LS_FLARE_PICKUP)
        || lara_item->current_anim_state == LS(LS_PICKUP)) {
        M_ControlInHand();
        lara_info->flare.control = false;
        lara_info->left_arm.frame_num = LF_FL_2_HOLD - 2;
        M_SetArm(lara_info->left_arm.frame_num);
        return;
    }

    int32_t frame_num = lara_info->left_arm.frame_num + 1;
    lara_info->flare.control = true;

    if (frame_num < LF_FL_DRAW || frame_num > LF_FL_2_HOLD - 1) {
        frame_num = LF_FL_DRAW;
    } else if (frame_num == LF_FL_DRAW_GOT_IT) {
        Lara_Flare_DrawMeshes();
        if (!Game_IsBonusFlagSet(GBF_NGPLUS)) {
            Inv_RemoveItem(O_FLAREBOX_ITEM);
        }
    } else if (frame_num >= LF_FL_IGNITE && frame_num <= LF_FL_2_HOLD - 2) {
        if (frame_num == LF_FL_IGNITE) {
            lara_info->flare.age = 0;
        }
        M_ControlInHand();
    } else if (frame_num == LF_FL_2_HOLD - 1) {
        M_InitialiseState();
        M_ControlInHand();
        frame_num = LF_FL_HOLD;
    }

    lara_info->left_arm.frame_num = frame_num;
    M_SetArm(frame_num);
}

void Lara_Flare_Undraw(void)
{
    ITEM *const lara_item = Lara_GetItem();
    LARA_INFO *const lara_info = Lara_GetLaraInfo();
    int16_t frame_num_1 = lara_info->left_arm.frame_num;
    int16_t frame_num_2 = lara_info->flare.frame_num;
    const bool is_mounted = Lara_Vehicle_IsMounted();

    lara_info->flare.control = true;

    if (lara_item->goal_anim_state == LS(LS_STOP) && !is_mounted) {
        if (Item_TestAnimEqual(lara_item, LA(LA_STAND_IDLE))) {
            int16_t throw_frame = frame_num_1;
            if (throw_frame < LF_FL_THROW || throw_frame >= LF_FL_DRAW) {
                throw_frame = LF_FL_THROW;
            }
            Item_SwitchToAnim(lara_item, LA(LA_FLARE_THROW), throw_frame);
            lara_info->flare.frame_num = lara_item->frame_num;
            frame_num_2 = lara_item->frame_num;
            frame_num_1 = throw_frame;
        }

        if (Item_TestAnimEqual(lara_item, LA(LA_FLARE_THROW))) {
            lara_info->flare.control = false;
            const OBJECT *const obj = Object_Get(O_LARA);
            const ANIM *const anim = Object_GetAnim(obj, LA(LA_FLARE_THROW));
            if (frame_num_2 >= anim->frame_base + LF_FL_THROW_FT - 1) {
                lara_info->gun_type = lara_info->last_gun_type;
                lara_info->request_gun_type = lara_info->last_gun_type;
                lara_info->gun_status = LGS_ARMLESS;
                Gun_InitialiseNewWeapon();
                lara_info->target = nullptr;
                lara_info->right_arm.lock = 0;
                lara_info->left_arm.lock = 0;
                Item_SwitchToAnim(lara_item, LA(LA_STAND_STILL), 0);
                lara_info->flare.frame_num = lara_item->frame_num;
                lara_item->current_anim_state = LS(LS_STOP);
                lara_item->goal_anim_state = LS(LS_STOP);
                return;
            }
            lara_info->flare.frame_num = frame_num_2 + 1;
        }
    } else if (lara_item->current_anim_state == LS(LS_STOP) && !is_mounted) {
        Item_SwitchToAnim(lara_item, LA(LA_STAND_STILL), 0);
    }

    if (frame_num_1 == LF_FL_HOLD) {
        frame_num_1 = LF_FL_THROW;
    } else if (frame_num_1 >= LF_FL_IGNITE && frame_num_1 < LF_FL_2_HOLD) {
        frame_num_1++;
        if (frame_num_1 == LF_FL_2_HOLD - 1) {
            frame_num_1 = LF_FL_THROW;
        }
    } else if (frame_num_1 >= LF_FL_THROW && frame_num_1 < LF_FL_DRAW) {
        frame_num_1++;
        if (frame_num_1 == LF_FL_THROW_RELEASE) {
            Lara_Flare_Dispose(true);
        } else if (frame_num_1 == LF_FL_DRAW) {
            frame_num_1 = 0;
            lara_info->gun_type = lara_info->last_gun_type;
            lara_info->request_gun_type = lara_info->last_gun_type;
            lara_info->gun_status = LGS_ARMLESS;
            Gun_InitialiseNewWeapon();
            lara_info->target = nullptr;
            lara_info->flare.control = false;
            lara_info->right_arm.lock = 0;
            lara_info->left_arm.lock = 0;
            lara_info->flare.frame_num = 0;
        }
    } else if (frame_num_1 >= LF_FL_2_HOLD && frame_num_1 < LF_FL_END) {
        frame_num_1++;
        if (frame_num_1 == LF_FL_END) {
            frame_num_1 = LF_FL_THROW;
        }
    }

    if (frame_num_1 >= LF_FL_THROW && frame_num_1 < LF_FL_THROW_RELEASE) {
        M_ControlInHand();
    }

    lara_info->left_arm.frame_num = frame_num_1;
    M_SetArm(frame_num_1);
}

void Lara_Flare_Dispose(const bool thrown)
{
    const ITEM *const lara_item = Lara_GetItem();
    LARA_INFO *const lara_info = Lara_GetLaraInfo();

    const int16_t item_num = Item_Create();
    if (item_num == NO_ITEM) {
        goto finish;
    }

    ITEM *const item = Item_Get(item_num);
    item->object_id = O_FLARE_ITEM;
    item->room_num = lara_item->room_num;

    XYZ_32 vec = {
        .x = -16,
        .y = 32,
        .z = 42,
    };
    Lara_GetJointAbsPosition(&vec, LM_HAND_L);

    const SECTOR *const sector = Room_GetSector(vec, &item->room_num);
    const int32_t height = Room_GetHeight(sector, vec);
    if (height < vec.y) {
        item->pos.x = lara_item->pos.x;
        item->pos.y = vec.y;
        item->pos.z = lara_item->pos.z;
        item->rot.y = -lara_item->rot.y;
        item->room_num = lara_item->room_num;
    } else {
        item->pos.x = vec.x;
        item->pos.y = vec.y;
        item->pos.z = vec.z;
        if (thrown) {
            item->rot.y = lara_item->rot.y;
        } else {
            item->rot.y = lara_item->rot.y - DEG_45;
        }
    }

    Item_Initialise(item_num);

    item->rot.z = 0;
    item->rot.x = 0;
    item->shade.value_1 = -1;

    if (thrown) {
        item->speed = lara_item->speed + 50;
        item->fall_speed = lara_item->fall_speed - 50;
    } else {
        item->speed = lara_item->speed + 10;
        item->fall_speed = lara_item->fall_speed + 50;
    }

    if (Flare_GenerateLight(item->pos, lara_info->flare.age)) {
        FlareItem_SetAge(item, lara_info->flare.age, true);
    } else {
        FlareItem_SetAge(item, lara_info->flare.age, false);
    }

    Item_AddActive(item_num);
    item->status = IS_ACTIVE;

finish:
    M_UndrawMeshes();
    if (!thrown) {
        lara_info->flare.control = false;
    }
}

bool Lara_Flare_IsMeshActive(void)
{
    const LARA_SKIN_EQUIPMENT *const equipment =
        Lara_Skin_GetEquipment(LM_HAND_L);
    return equipment->type == EQUIPMENT_TYPE_WEAPON
        && equipment->data == LGT_FLARE;
}

void Lara_Flare_DrawMeshes(void)
{
    Lara_Skin_SetGunEquipment(LM_HAND_L, LGT_FLARE);
}
