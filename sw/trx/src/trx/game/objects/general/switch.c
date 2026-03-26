#include <trx/game/objects/general/switch.h>

#include <trx/config.h>
#include <trx/game/const.h>
#include <trx/game/input.h>
#include <trx/game/items.h>
#include <trx/game/lara.h>
#include <trx/game/objects.h>

typedef struct {
    XYZ_32 normal;
    XYZ_32 controlled;
} M_SWITCH_POS;

static const OBJECT_BOUNDS m_SwitchBounds = {
    .shift = {
        .min = { .x = -220, .y = +0, .z = +WALL_L / 2 - 220, },
        .max = { .x = +220, .y = +0, .z = +WALL_L / 2, },
    },
    .rot = {
        .min = { .x = -10 * DEG_1, .y = -30 * DEG_1, .z = -10 * DEG_1, },
        .max = { .x = +10 * DEG_1, .y = +30 * DEG_1, .z = +10 * DEG_1, },
    },
};

static const OBJECT_BOUNDS m_SwitchBoundsControlled = {
    .shift = {
        .min = { .x = -WALL_L / 2, .y = +0, .z = -200, },
        .max = { .x = +WALL_L / 2, .y = +0, .z = +200, },
    },
    .rot = {
        .min = { .x = -10 * DEG_1, .y = -30 * DEG_1, .z = -10 * DEG_1, },
        .max = { .x = +10 * DEG_1, .y = +30 * DEG_1, .z = +10 * DEG_1, },
    },
};

static const OBJECT_BOUNDS m_SwitchBoundsUW = {
    .shift = {
        .min = { .x = -WALL_L, .y = -WALL_L, .z = -WALL_L, },
        .max = { .x = +WALL_L, .y = +WALL_L, .z = +WALL_L / 2, },
    },
    .rot = {
        .min = { .x = -80 * DEG_1, .y = -80 * DEG_1, .z = -80 * DEG_1, },
        .max = { .x = +80 * DEG_1, .y = +80 * DEG_1, .z = +80 * DEG_1, },
    },
};

static const XYZ_32 m_SwitchUWPosition = { .x = 0, .y = 0, .z = 108 };

static const M_SWITCH_POS m_SmallSwitchPosition = {
    .normal = { .x = 0, .y = 0, .z = 362 },
    .controlled = { .x = 0, .y = 0, .z = 80 },
};

static const M_SWITCH_POS m_PushSwitchPosition = {
    .normal = { .x = 0, .y = 0, .z = 292 },
    .controlled = { .x = 0, .y = 0, .z = 146 },
};

static const M_SWITCH_POS m_WallSwitchPosition = {
    .normal = { .x = 0, .y = 0, .z = 128 },
    .controlled = { .x = 0, .y = 0, .z = 64 },
};

static const M_SWITCH_POS m_AirlockPosition = {
    .normal = { .x = 0, .y = 0, .z = 212 },
    .controlled = { .x = 0, .y = 0, .z = 106 },
};

static const OBJECT_BOUNDS *M_Bounds(void)
{
    return g_Config.gameplay.enable_walk_to_items ? &m_SwitchBoundsControlled
                                                  : &m_SwitchBounds;
}

static const OBJECT_BOUNDS *M_BoundsUW(void)
{
    return &m_SwitchBoundsUW;
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    item->flags |= IF_CODE_BITS;
    if (!Item_IsTriggerActive(item)) {
        item->goal_anim_state = SWITCH_STATE_OFF;
        item->timer = 0;
    }
    Item_Animate(item);
}

static void M_AlignLara(ITEM *const lara_item, ITEM *const switch_item)
{
    lara_item->rot.y = switch_item->rot.y;
    switch (switch_item->object_id) {
    case O_SWITCH_TYPE_AIRLOCK:
    case O_SWITCH_TYPE_WHEEL:
        Lara_AlignPosition(switch_item, &m_AirlockPosition.normal);
        break;

    case O_SWITCH_TYPE_SMALL:
        Lara_AlignPosition(switch_item, &m_SmallSwitchPosition.normal);
        break;

    case O_SWITCH_TYPE_BUTTON:
        Lara_AlignPosition(switch_item, &m_PushSwitchPosition.normal);
        break;

    default:
        break;
    }
}

static bool M_MoveLaraControlled(
    const ITEM *const item, const BOUNDS_16 *const bounds)
{
    XYZ_32 shift;
    switch (item->object_id) {
    case O_SWITCH_TYPE_AIRLOCK:
    case O_SWITCH_TYPE_WHEEL:
        shift = m_AirlockPosition.controlled;
        break;
    case O_SWITCH_TYPE_SMALL:
        shift = m_SmallSwitchPosition.controlled;
        break;
    case O_SWITCH_TYPE_BUTTON:
        shift = m_PushSwitchPosition.controlled;
        break;
    default:
        shift = m_WallSwitchPosition.controlled;
        break;
    }

    const XYZ_32 move_vector = {
        .x = 0,
        .y = 0,
        .z = bounds->min.z - shift.z,
    };
    return Lara_MovePosition(item, &move_vector);
}

static void M_TurnSwitchOn(ITEM *const switch_item, ITEM *const lara_item)
{
    switch (switch_item->object_id) {
    case O_SWITCH_TYPE_WHEEL:
        Lara_SwitchToExtraState(LS_EXTRA_AIRLOCK);
        break;
    case O_SWITCH_TYPE_SMALL:
        Item_SwitchToAnim(lara_item, LA(LA_SWITCH_SMALL_DOWN), 0);
        break;

    case O_SWITCH_TYPE_BUTTON:
        Item_SwitchToAnim(lara_item, LA(LA_BUTTON_PUSH), 0);
        break;

    default:
        Item_SwitchToAnim(lara_item, LA(LA_WALL_SWITCH_DOWN), 0);
        break;
    }

    if (!Lara_GetLaraInfo()->extra_anim) {
        lara_item->current_anim_state = LS(LS_SWITCH_ON);
    }
    switch_item->goal_anim_state = SWITCH_STATE_ON;
}

static void M_TurnSwitchOff(ITEM *const switch_item, ITEM *const lara_item)
{
    lara_item->current_anim_state = LS(LS_SWITCH_OFF);

    LARA_INFO *const lara = Lara_GetLaraInfo();
    switch (switch_item->object_id) {
    case O_SWITCH_TYPE_AIRLOCK:
    case O_SWITCH_TYPE_WHEEL:
        Lara_SwitchToExtraState(LS_EXTRA_AIRLOCK);
        break;

    case O_SWITCH_TYPE_SMALL:
        Item_SwitchToAnim(lara_item, LA(LA_SWITCH_SMALL_UP), 0);
        break;

    case O_SWITCH_TYPE_BUTTON:
        Item_SwitchToAnim(lara_item, LA(LA_BUTTON_PUSH), 0);
        break;

    default:
        Item_SwitchToAnim(lara_item, LA(LA_WALL_SWITCH_UP), 0);
        break;
    }

    switch_item->goal_anim_state = SWITCH_STATE_OFF;
}

static void M_CollisionControlled(
    const int16_t item_num, ITEM *const lara_item, COLL_INFO *const coll)
{
    ITEM *const item = Item_Get(item_num);
    LARA_INFO *const lara = Lara_GetLaraInfo();

    if ((g_Input.action && lara->gun_status == LGS_ARMLESS
         && !lara_item->gravity && lara_item->current_anim_state == LS(LS_STOP)
         && item->status == IS_INACTIVE)
        || (lara->interact_target.is_moving
            && lara->interact_target.item_num == item_num)) {
        const BOUNDS_16 *const bounds = Item_GetBoundsAccurate(item);

        OBJECT_BOUNDS col_bounds = *Object_Get(item->object_id)->bounds_func();
        col_bounds.shift.min.x += bounds->min.x;
        col_bounds.shift.max.x += bounds->max.x;
        col_bounds.shift.min.z += bounds->min.z;
        col_bounds.shift.max.z += bounds->max.z;

        if (Lara_TestPosition(item, &col_bounds)) {
            if (M_MoveLaraControlled(item, bounds)) {
                if (item->current_anim_state == SWITCH_STATE_OFF) {
                    M_TurnSwitchOn(item, lara_item);
                } else {
                    M_TurnSwitchOff(item, lara_item);
                }
                lara->head_rot.x = 0;
                lara->head_rot.y = 0;
                lara->torso_rot.x = 0;
                lara->torso_rot.y = 0;
                lara->interact_target.is_moving = false;
                lara->interact_target.item_num = NO_ITEM;
                lara->gun_status = LGS_HANDS_BUSY;
                Item_AddActive(item_num);
                item->status = IS_ACTIVE;
                Item_Animate(item);
            } else {
                lara->interact_target.item_num = item_num;
            }
        } else if (
            lara->interact_target.is_moving
            && lara->interact_target.item_num == item_num) {
            lara->interact_target.is_moving = false;
            lara->gun_status = LGS_ARMLESS;
        }
    } else if (
        lara_item->current_anim_state != LS(LS_SWITCH_ON)
        && lara_item->current_anim_state != LS(LS_SWITCH_OFF)) {
        Object_Collision(item_num, lara_item, coll);
    }
}

static void M_Collision(
    const int16_t item_num, ITEM *const lara_item, COLL_INFO *const coll)
{
    if (g_Config.gameplay.enable_walk_to_items) {
        M_CollisionControlled(item_num, lara_item, coll);
        return;
    }

    ITEM *const item = Item_Get(item_num);
    LARA_INFO *const lara = Lara_GetLaraInfo();
    const OBJECT *const obj = Object_Get(item->object_id);

    if (!g_Input.action || item->status != IS_INACTIVE
        || lara->gun_status != LGS_ARMLESS || lara_item->gravity
        || lara_item->current_anim_state != LS(LS_STOP)
        || !Lara_TestPosition(item, obj->bounds_func())) {
        return;
    }

    if (item->object_id == O_SWITCH_TYPE_AIRLOCK
        && item->current_anim_state == SWITCH_STATE_OFF) {
        return;
    }

    M_AlignLara(lara_item, item);

    if (item->current_anim_state == SWITCH_STATE_OFF) {
        M_TurnSwitchOn(item, lara_item);
    } else {
        M_TurnSwitchOff(item, lara_item);
    }

    if (!lara->extra_anim) {
        lara_item->goal_anim_state = LS(LS_STOP);
    }
    lara->gun_status = LGS_HANDS_BUSY;

    item->status = IS_ACTIVE;
    Item_AddActive(item_num);
    Item_Animate(item);
}

static void M_CollisionUW(
    const int16_t item_num, ITEM *const lara_item, COLL_INFO *const coll)
{
    ITEM *const item = Item_Get(item_num);
    LARA_INFO *const lara = Lara_GetLaraInfo();
    const OBJECT *const obj = Object_Get(item->object_id);

    if (!g_Input.action || item->status != IS_INACTIVE
        || (lara->water_status != LWS_UNDERWATER
            && lara->water_status != LWS_CHEAT)
        || lara->gun_status != LGS_ARMLESS
        || lara_item->current_anim_state != LS(LS_TREAD)) {
        return;
    }

    if (!Lara_TestPosition(item, obj->bounds_func())) {
        return;
    }

    if (item->current_anim_state != SWITCH_STATE_ON
        && item->current_anim_state != SWITCH_STATE_OFF) {
        return;
    }

    if (!Lara_MovePosition(item, &m_SwitchUWPosition)) {
        return;
    }

    lara_item->fall_speed = 0;
    Lara_AnimateUntil(lara_item, LS(LS_SWITCH_ON));
    lara_item->goal_anim_state = LS(LS_TREAD);
    lara->gun_status = LGS_HANDS_BUSY;

    if (item->current_anim_state == SWITCH_STATE_OFF) {
        item->goal_anim_state = SWITCH_STATE_ON;
    } else {
        item->goal_anim_state = SWITCH_STATE_OFF;
    }
    item->status = IS_ACTIVE;
    Item_AddActive(item_num);
    Item_Animate(item);
}

static void M_SetupBase(OBJECT *const obj)
{
    obj->control_func = M_Control;
    obj->save_flags = true;
    obj->save_anim = true;
}

static void M_SetupCommon(OBJECT *const obj)
{
    M_SetupBase(obj);
    obj->collision_func = M_Collision;
    obj->bounds_func = M_Bounds;
}

static void M_SetupPushButton(OBJECT *const obj)
{
    M_SetupCommon(obj);
    obj->enable_interpolation = false;
    obj->bounds_func = M_Bounds;
}

static void M_SetupUW(OBJECT *const obj)
{
    M_SetupBase(obj);
    obj->collision_func = M_CollisionUW;
    obj->bounds_func = M_BoundsUW;
}

static void M_SetupAirlock(OBJECT *const obj)
{
    M_SetupCommon(obj);
    obj->draw_func = Object_DrawUnclippedItem;
}

bool Switch_Trigger(const int16_t item_num, const int16_t timer)
{
    ITEM *const item = Item_Get(item_num);

    if (item->object_id == O_SWITCH_TYPE_AIRLOCK) {
        if (item->status == IS_DEACTIVATED) {
            Item_RemoveActive(item_num);
            item->status = IS_INACTIVE;
            return false;
        } else if (
            (item->flags & IF_ONE_SHOT) != 0
            || item->current_anim_state == SWITCH_STATE_ON) {
            return false;
        }

        item->flags |= IF_ONE_SHOT;
        return true;
    }

    if (item->status != IS_DEACTIVATED) {
        return false;
    }

    if (item->current_anim_state == SWITCH_STATE_ON && timer > 0) {
        item->timer = timer;
        if (timer != 1) {
            item->timer *= LOGIC_FPS;
        }
        item->status = IS_ACTIVE;
    } else {
        Item_RemoveActive(item_num);
        item->status = IS_INACTIVE;
    }
    return true;
}

REGISTER_OBJECT(O_SWITCH_TYPE_AIRLOCK, M_SetupAirlock)
REGISTER_OBJECT(O_SWITCH_TYPE_BUTTON, M_SetupPushButton)
REGISTER_OBJECT(O_SWITCH_TYPE_NORMAL, M_SetupCommon)
REGISTER_OBJECT(O_SWITCH_TYPE_SMALL, M_SetupCommon)
REGISTER_OBJECT(O_SWITCH_TYPE_UW, M_SetupUW)
REGISTER_OBJECT(O_SWITCH_TYPE_WHEEL, M_SetupCommon)
