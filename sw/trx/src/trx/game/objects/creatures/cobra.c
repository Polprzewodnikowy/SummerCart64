#include <trx/core/utils.h>
#include <trx/game/creature.h>
#include <trx/game/game_flow.h>
#include <trx/game/lara.h>
#include <trx/game/objects.h>
#include <trx/game/spawn.h>
#include <trx/version.h>

static BITE m_CobraBite = { .pos = { 0, 0, 0 }, .mesh_num = 13 };

typedef enum {
    COBRA_STATE_WAKING_UP = 0,
    COBRA_STATE_ALERT = 1,
    COBRA_STATE_BITE = 2,
    COBRA_STATE_SLEEP = 3,
    COBRA_STATE_DEATH = 4,
} M_COBRA_STATE;

typedef enum {
    COBRA_ANIM_SLEEP = 2,
    COBRA_ANIM_DEATH = 4,
} M_COBRA_ANIM;

static void M_Initialise(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    Creature_Initialise(item_num);
    Item_SwitchToAnim(item, COBRA_ANIM_SLEEP, 45);
    item->current_anim_state = COBRA_STATE_SLEEP;
    item->goal_anim_state = COBRA_STATE_SLEEP;
}

static bool M_IsTargetable(const ITEM *const item)
{
    return item->hit_points > 0 && item->status == IS_ACTIVE
        && item->current_anim_state != COBRA_STATE_SLEEP;
}

static bool M_CanTakeDamage(const ITEM *const item)
{
    return item->hit_points > 0;
}

static bool M_CanBeProjectileTarget(const ITEM *const item)
{
    return item->hit_points > 0 && item->collidable;
}

static void M_Control(const int16_t item_num)
{
    if (!Creature_Activate(item_num)) {
        return;
    }

    int32_t forget_radius = SQUARE(3 * WALL_L);
    int32_t alert_radius = SQUARE(1.5 * WALL_L);
    int32_t attack_radius = SQUARE(WALL_L);

    // TODO: do not hardcode this
    if (g_TRVersion == 3 && GF_BadGetLevelNum() >= 9) {
        forget_radius = SQUARE(2.5 * WALL_L);
        alert_radius = SQUARE(1.25 * WALL_L);
        attack_radius = SQUARE(682);
    }

    ITEM *const item = Item_Get(item_num);
    CREATURE *const creature = item->creature_data;

    if (creature == nullptr) {
        return;
    }

    int16_t angle = 0;
    if (item->hit_points <= 0) {
        if (item->current_anim_state != COBRA_STATE_DEATH) {
            Item_SwitchToAnim(item, COBRA_ANIM_DEATH, 0);
            item->current_anim_state = COBRA_ANIM_DEATH;
        }

        goto finish;
    }

    const ITEM *const lara_item = Lara_GetItem();
    LARA_INFO *const lara = Lara_GetLaraInfo();

    AI_INFO info;
    Creature_AIInfo(item, &info);
    info.angle += 3072;
    creature->target.x = lara_item->pos.x;
    creature->target.z = lara_item->pos.z;
    angle = Creature_Turn(item, creature->maximum_turn);

    if (ABS(info.angle) < DEG_1 * 10) {
        item->rot.y += info.angle;
    } else if (info.angle < 0) {
        item->rot.y -= DEG_1 * 10;
    } else {
        item->rot.y += DEG_1 * 10;
    }

    switch (item->current_anim_state) {
    case COBRA_STATE_WAKING_UP:
        break;

    case COBRA_STATE_ALERT:
        creature->flags = 0;
        if (info.distance > forget_radius) {
            item->goal_anim_state = COBRA_STATE_SLEEP;
        } else if (
            lara_item->hit_points > 0
            && ((info.ahead && info.distance < attack_radius)
                || item->hit_status || lara_item->speed > 15)) {
            item->goal_anim_state = COBRA_STATE_BITE;
        }
        break;

    case COBRA_STATE_BITE:
        if (creature->flags != 1 && (item->touch_bits & 0x2000) != 0) {
            creature->flags = 1;
            Lara_TakeDamage(80, true);
            lara->poison_timer = 256;
            Creature_Effect(item, &m_CobraBite, Spawn_Blood);
        }
        break;

    case COBRA_STATE_SLEEP:
        creature->flags = 0;
        if (info.distance < alert_radius && lara_item->hit_points > 0) {
            item->goal_anim_state = COBRA_STATE_WAKING_UP;
        }
        break;
    }

finish:
    Creature_Animate(item_num, angle, 0);
}

static void M_Setup(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }

    obj->initialise_func = M_Initialise;
    obj->control_func = M_Control;
    obj->collision_func = Creature_Collision;
    obj->is_targetable_func = M_IsTargetable;
    obj->can_take_damage_func = M_CanTakeDamage;
    obj->can_be_projectile_target_func = M_CanBeProjectileTarget;

    obj->shadow_size = UNIT_SHADOW / 2;
    obj->hit_points = 8;
    obj->radius = 102;

    // obj->non_lot = true; // TODO(TR3)
    obj->intelligent = true;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_flags = true;
    obj->save_anim = true;

    Object_GetBone(obj, 0)->rot.y = true;
    Object_GetBone(obj, 6)->rot.y = true;
}

REGISTER_OBJECT(O_COBRA, M_Setup)
