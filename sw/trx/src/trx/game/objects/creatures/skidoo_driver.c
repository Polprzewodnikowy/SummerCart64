#include <trx/game/objects/creatures/skidoo_driver.h>

#include <trx/core/utils.h>
#include <trx/debug.h>
#include <trx/game/creature.h>
#include <trx/game/items/carrier.h>
#include <trx/game/lara.h>
#include <trx/game/objects/vehicles/skidoo_common.h>
#include <trx/game/pathing.h>
#include <trx/game/sound.h>

#define SKIDOO_DRIVER_MIN_TURN (SKIDOO_MAX_TURN / 3) // = 364
#define SKIDOO_DRIVER_TARGET_ANGLE (DEG_1 * 15) // = 2730
#define SKIDOO_DRIVER_WAIT_RANGE SQUARE(WALL_L * 4) // = 0x1000000
#define SKIDOO_DRIVER_SHOT_DAMAGE 10
#define SKIDOO_DRIVER_LARA_DAMAGE 50

typedef enum {
    // clang-format off
    SKIDOO_DRIVER_STATE_EMPTY       = 0,
    SKIDOO_DRIVER_STATE_WAIT        = 1,
    SKIDOO_DRIVER_STATE_MOVING      = 2,
    SKIDOO_DRIVER_STATE_START_LEFT  = 3,
    SKIDOO_DRIVER_STATE_START_RIGHT = 4,
    SKIDOO_DRIVER_STATE_LEFT        = 5,
    SKIDOO_DRIVER_STATE_RIGHT       = 6,
    SKIDOO_DRIVER_STATE_DEATH       = 7,
    // clang-format on
} SKIDOO_DRIVER_STATE;

typedef enum {
    SKIDOO_DRIVER_ANIM_DEATH = 10,
} SKIDOO_DRIVER_ANIM;

typedef struct {
    int16_t skidoo_item_num;
} M_PRIV;

static void M_KillDriver(ITEM *const driver_item)
{
    const int32_t driver_item_num = Item_GetIndex(driver_item);
    Item_RemoveActive(driver_item_num);
    driver_item->collidable = 0;
    driver_item->flags |= IF_ONE_SHOT;
    driver_item->hit_points = 0;
}

static void M_MakeMountable(ITEM *const skidoo_item)
{
    if (skidoo_item->status == IS_INVISIBLE) {
        return;
    }

    const int32_t skidoo_item_num = Item_GetIndex(skidoo_item);
    LOT_DisableBaddieAI(skidoo_item_num);
    skidoo_item->object_id = O_SKIDOO_FAST;
    skidoo_item->status = IS_DEACTIVATED;
    Skidoo_Initialise(skidoo_item_num);

    SKIDOO_INFO *const skidoo_data = skidoo_item->priv;
    skidoo_data->track_mesh = SKIDOO_GUN_MESH;
}

static void M_ControlDead(ITEM *const driver_item, ITEM *const skidoo_item)
{
    if (driver_item->current_anim_state == SKIDOO_DRIVER_STATE_DEATH) {
        Item_Animate(driver_item);
    } else {
        driver_item->pos.x = skidoo_item->pos.x;
        driver_item->pos.y = skidoo_item->pos.y;
        driver_item->pos.z = skidoo_item->pos.z;
        driver_item->rot.y = skidoo_item->rot.y;
        driver_item->room_num = skidoo_item->room_num;
        Item_SwitchToAnim(driver_item, SKIDOO_DRIVER_ANIM_DEATH, 0);
        driver_item->current_anim_state = SKIDOO_DRIVER_STATE_DEATH;
        Carrier_TestItemDrops(Item_GetIndex(skidoo_item));

        LARA_INFO *const lara = Lara_GetLaraInfo();
        if (lara->target == skidoo_item) {
            lara->target = nullptr;
        }
    }

    switch (skidoo_item->current_anim_state) {
    case SKIDOO_DRIVER_STATE_MOVING:
    case SKIDOO_DRIVER_STATE_WAIT:
        skidoo_item->goal_anim_state = SKIDOO_DRIVER_STATE_WAIT;
        break;
    default:
        skidoo_item->goal_anim_state = SKIDOO_DRIVER_STATE_MOVING;
        break;
    }
}

static int16_t M_ControlAlive(ITEM *const driver_item, ITEM *const skidoo_item)
{
    CREATURE *const driver_data = skidoo_item->creature_data;

    AI_INFO info;
    Creature_AIInfo(skidoo_item, &info);
    Creature_Mood(skidoo_item, &info, true);

    int16_t angle = Creature_Turn(skidoo_item, SKIDOO_MAX_TURN / 2);

    switch (skidoo_item->current_anim_state) {
    case SKIDOO_DRIVER_STATE_WAIT:
        if (driver_data->mood != MOOD_BORED
            && (ABS(info.angle) >= SKIDOO_DRIVER_TARGET_ANGLE
                || info.distance >= SKIDOO_DRIVER_WAIT_RANGE)) {
            skidoo_item->goal_anim_state = SKIDOO_DRIVER_STATE_MOVING;
        }
        break;

    case SKIDOO_DRIVER_STATE_MOVING:
        if (driver_data->mood == MOOD_BORED) {
            skidoo_item->goal_anim_state = SKIDOO_DRIVER_STATE_WAIT;
        } else if (
            ABS(info.angle) < SKIDOO_DRIVER_TARGET_ANGLE
            && info.distance < SKIDOO_DRIVER_WAIT_RANGE) {
            skidoo_item->goal_anim_state = SKIDOO_DRIVER_STATE_WAIT;
        } else if (angle < -SKIDOO_DRIVER_MIN_TURN) {
            skidoo_item->goal_anim_state = SKIDOO_DRIVER_STATE_START_LEFT;
        } else if (angle > SKIDOO_DRIVER_MIN_TURN) {
            skidoo_item->goal_anim_state = SKIDOO_DRIVER_STATE_START_RIGHT;
        }
        break;

    case SKIDOO_DRIVER_STATE_START_LEFT:
    case SKIDOO_DRIVER_STATE_LEFT:
        if (angle >= -SKIDOO_DRIVER_MIN_TURN) {
            skidoo_item->goal_anim_state = SKIDOO_DRIVER_STATE_MOVING;
        } else {
            skidoo_item->goal_anim_state = SKIDOO_DRIVER_STATE_LEFT;
        }
        break;

    case SKIDOO_DRIVER_STATE_START_RIGHT:
    case SKIDOO_DRIVER_STATE_RIGHT:
        if (angle >= -SKIDOO_DRIVER_MIN_TURN) {
            skidoo_item->goal_anim_state = SKIDOO_DRIVER_STATE_MOVING;
        } else {
            skidoo_item->goal_anim_state = SKIDOO_DRIVER_STATE_LEFT;
        }
        break;
    }

    if (driver_item->current_anim_state != SKIDOO_DRIVER_STATE_DEATH) {
        const ITEM *const lara_item = Lara_GetItem();
        if (driver_data->flags == 0
            && ABS(info.angle) < SKIDOO_DRIVER_TARGET_ANGLE
            && lara_item->hit_points > 0) {
            const int32_t damage = Lara_Vehicle_IsMounted()
                ? SKIDOO_DRIVER_SHOT_DAMAGE
                : SKIDOO_DRIVER_LARA_DAMAGE;

            const bool left_targetable = Creature_Shoot(
                skidoo_item, &info,
                &(CREATURE_GUN) {
                    .muzzle = g_Skidoo_RightGun,
                },
                0, damage);
            const bool right_targetable = Creature_Shoot(
                skidoo_item, &info,
                &(CREATURE_GUN) {
                    .muzzle = g_Skidoo_LeftGun,
                },
                0, damage);
            if (left_targetable || right_targetable) {
                driver_data->flags = 5;
            }
        }

        if (driver_data->flags != 0) {
            Sound_Effect(SFX_LARA_UZI_FIRE, &skidoo_item->pos, SPM_NORMAL);
            driver_data->flags--;
        }
    }

    return angle;
}

static void M_Initialise(const int16_t item_num)
{
    ITEM *const skidoo_driver = Item_Get(item_num);
    M_PRIV *const p = skidoo_driver->priv;

    const int16_t skidoo_item_num = Item_CreateLevelItem();
    ASSERT(skidoo_item_num != NO_ITEM);

    ITEM *const skidoo = Item_Get(skidoo_item_num);
    skidoo->object_id = O_SKIDOO_ARMED;
    skidoo->pos.x = skidoo_driver->pos.x;
    skidoo->pos.y = skidoo_driver->pos.y;
    skidoo->pos.z = skidoo_driver->pos.z;
    skidoo->rot.y = skidoo_driver->rot.y;
    skidoo->room_num = skidoo_driver->room_num;
    skidoo->flags = IF_ONE_SHOT;
    skidoo->shade.value_1 = -1;
    Item_Initialise(skidoo_item_num);

    p->skidoo_item_num = skidoo_item_num;
}

static void M_HandleSave(ITEM *const item, const SAVEGAME_STAGE stage)
{
    if (stage == SAVEGAME_STAGE_AFTER_LOAD) {
        if (item->status == IS_DEACTIVATED) {
            item->hit_points = 0;
            const M_PRIV *const p = item->priv;
            const int16_t skidoo_num = p->skidoo_item_num;
            ITEM *const skidoo = Item_Get(skidoo_num);
            skidoo->object_id = O_SKIDOO_FAST;
            Skidoo_Initialise(skidoo_num);
        }
    }
}

static void M_Control(const int16_t driver_item_num)
{
    ITEM *const driver_item = Item_Get(driver_item_num);
    const M_PRIV *const p = driver_item->priv;
    const int16_t skidoo_item_num = p->skidoo_item_num;
    ITEM *const skidoo_item = Item_Get(skidoo_item_num);

    if (skidoo_item->creature_data == nullptr) {
        LOT_EnableBaddieAI(skidoo_item_num, true);
        skidoo_item->status = IS_ACTIVE;
    }

    CREATURE *const driver_data = skidoo_item->creature_data;
    int16_t angle = 0;

    if (skidoo_item->hit_points <= 0) {
        M_ControlDead(driver_item, skidoo_item);
    } else {
        angle = M_ControlAlive(driver_item, skidoo_item);
    }

    if (skidoo_item->current_anim_state == SKIDOO_DRIVER_STATE_WAIT) {
        driver_data->head_rotation = 0;
        Sound_Effect(SFX_SKIDOO_IDLE, &skidoo_item->pos, SPM_NORMAL);
    } else {
        driver_data->head_rotation = driver_data->head_rotation == 1 ? 2 : 1;
        if (skidoo_item->status != IS_INVISIBLE) {
            Skidoo_DoSnowEffect(skidoo_item);
        }

        const int32_t pitch_delta =
            (SKIDOO_MAX_SPEED - skidoo_item->speed) * 100;
        const int32_t pitch = (SOUND_DEFAULT_PITCH - pitch_delta) << 8;
        Sound_Effect(SFX_SKIDOO_MOVING, &skidoo_item->pos, SPM_PITCH | pitch);
    }

    Creature_Animate(skidoo_item_num, angle, 0);

    if (driver_item->current_anim_state == SKIDOO_DRIVER_STATE_DEATH) {
        if (driver_item->status == IS_DEACTIVATED && skidoo_item->speed == 0
            && skidoo_item->fall_speed == 0) {
            M_KillDriver(driver_item);
            M_MakeMountable(skidoo_item);
        }
    } else {
        driver_item->pos.x = skidoo_item->pos.x;
        driver_item->pos.y = skidoo_item->pos.y;
        driver_item->pos.z = skidoo_item->pos.z;
        driver_item->rot.y = skidoo_item->rot.y;
        Item_UpdateRoom(driver_item_num, skidoo_item->room_num);
        const int16_t anim_num =
            Item_GetRelativeObjAnim(skidoo_item, O_SKIDOO_ARMED);
        const int16_t frame_num = Item_GetRelativeFrame(skidoo_item);
        Item_SwitchToAnim(driver_item, anim_num, frame_num);
    }
}

static bool M_IsTargetable(const ITEM *const item)
{
    return false;
}

static void M_Setup(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }

    obj->initialise_func = M_Initialise;
    obj->handle_save_func = M_HandleSave;
    obj->control_func = M_Control;
    obj->priv_size = sizeof(M_PRIV);
    obj->is_targetable_func = M_IsTargetable;

    obj->hit_points = 1;

    obj->save_position = true;
    obj->save_flags = true;
    obj->save_anim = true;
}

int16_t SkidooDriver_GetSkidooItemNum(const ITEM *const driver_item)
{
    const M_PRIV *const p = driver_item->priv;
    return p->skidoo_item_num;
}

REGISTER_OBJECT(O_SKIDOO_DRIVER, M_Setup)
