#include <trx/core/utils.h>
#include <trx/game/creature.h>
#include <trx/game/objects/common.h>
#include <trx/game/objects/creatures/worker_common.h>
#include <trx/game/random.h>
#include <trx/game/spawn.h>

#define WORKER_2_HITPOINTS 20
#define WORKER_5_HITPOINTS 20

// clang-format off
#define WORKER_2_SHOOT_DAMAGE  30
#define WORKER_2_WALK_TURN     (DEG_1 * 3) // = 546
#define WORKER_2_RUN_TURN      (DEG_1 * 5) // = 910
#define WORKER_2_RUN_RANGE     SQUARE(WALL_L * 2) // = 4194304
#define WORKER_2_SHOOT_1_RANGE SQUARE(WALL_L * 3) // = 9437184
// clang-format on

typedef enum {
    // clang-format off
    WORKER_2_STATE_EMPTY   = 0,
    WORKER_2_STATE_STOP    = 1,
    WORKER_2_STATE_WALK    = 2,
    WORKER_2_STATE_RUN     = 3,
    WORKER_2_STATE_WAIT    = 4,
    WORKER_2_STATE_SHOOT_1 = 5,
    WORKER_2_STATE_SHOOT_2 = 6,
    WORKER_2_STATE_DEATH   = 7,
    WORKER_2_STATE_AIM_1   = 8,
    WORKER_2_STATE_AIM_2   = 9,
    WORKER_2_STATE_AIM_3   = 10,
    WORKER_2_STATE_SHOOT_3 = 11,
    // clang-format on
} WORKER_2_STATE;

typedef enum {
    WORKER_2_ANIM_DEATH = 19,
} WORKER_2_ANIM;

static const CREATURE_GUN m_Worker2Gun = {
    .muzzle = { .pos = { .x = 0, .y = 308, .z = 32 }, .mesh_num = 9 },
};

static void M_ShootAtLara(
    ITEM *const item, CREATURE *const creature, const AI_INFO *const info,
    const int16_t head)
{
    if (item->object_id == O_WORKER_2) {
        if (creature->flags != 0) {
            creature->flags--;
        } else {
            Creature_Shoot(
                item, info, &m_Worker2Gun, head, WORKER_2_SHOOT_DAMAGE);
            creature->flags = 5;
        }
    } else {
        Creature_Effect(item, &m_Worker2Gun.muzzle, Spawn_FireStream);
    }
}

static void M_Control(const int16_t item_num)
{
    if (!Creature_Activate(item_num)) {
        return;
    }

    ITEM *const item = Item_Get(item_num);
    CREATURE *const creature = item->creature_data;

    int16_t head = 0;
    int16_t tilt = 0;
    int16_t neck = 0;
    int16_t angle = 0;

    if (item->hit_points <= 0) {
        if (item->current_anim_state != WORKER_2_STATE_DEATH) {
            Item_SwitchToAnim(item, WORKER_2_ANIM_DEATH, 0);
            item->current_anim_state = WORKER_2_STATE_DEATH;
        }
    } else {
        AI_INFO info;
        Creature_AIInfo(item, &info);
        Creature_Mood(item, &info, false);

        angle = Creature_Turn(item, creature->maximum_turn);

        switch (item->current_anim_state) {
        case WORKER_2_STATE_STOP:
            if (info.ahead) {
                neck = info.angle;
            }
            creature->flags = 0;
            creature->maximum_turn = 0;
            if (creature->mood == MOOD_ESCAPE) {
                item->goal_anim_state = WORKER_2_STATE_RUN;
            } else if (Creature_CanTargetEnemy(item, &info)) {
                if (info.distance >= WORKER_2_SHOOT_1_RANGE
                    && info.zone_num == info.enemy_zone_num) {
                    item->goal_anim_state = WORKER_2_STATE_WALK;
                } else if (
                    item->object_id == O_WORKER_5
                    || Random_GetControl() < 0x4000) {
                    item->goal_anim_state = WORKER_2_STATE_AIM_1;
                } else {
                    item->goal_anim_state = WORKER_2_STATE_AIM_3;
                }
            } else if (creature->mood == MOOD_BORED && info.ahead) {
                item->goal_anim_state = WORKER_2_STATE_WAIT;
            } else if (info.distance > WORKER_2_RUN_RANGE) {
                item->goal_anim_state = WORKER_2_STATE_RUN;
            } else {
                item->goal_anim_state = WORKER_2_STATE_WALK;
            }
            break;

        case WORKER_2_STATE_WAIT:
            if (info.ahead) {
                neck = info.angle;
            }
            if (Creature_CanTargetEnemy(item, &info)) {
                item->goal_anim_state = WORKER_2_STATE_SHOOT_1;
            } else if (creature->mood != MOOD_BORED || !info.ahead) {
                item->goal_anim_state = WORKER_2_STATE_STOP;
            }
            break;

        case WORKER_2_STATE_WALK:
            if (info.ahead) {
                neck = info.angle;
            }
            creature->flags = 0;
            creature->maximum_turn = WORKER_2_WALK_TURN;
            if (creature->mood == MOOD_ESCAPE) {
                item->goal_anim_state = WORKER_2_STATE_RUN;
            } else if (Creature_CanTargetEnemy(item, &info)) {
                if (info.distance < WORKER_2_SHOOT_1_RANGE
                    || info.zone_num != info.enemy_zone_num) {
                    item->goal_anim_state = WORKER_2_STATE_STOP;
                } else {
                    item->goal_anim_state = WORKER_2_STATE_AIM_2;
                }
            } else if (creature->mood == MOOD_BORED && info.ahead) {
                item->goal_anim_state = WORKER_2_STATE_STOP;
            } else if (info.distance > WORKER_2_RUN_RANGE) {
                item->goal_anim_state = WORKER_2_STATE_RUN;
            }
            break;

        case WORKER_2_STATE_RUN:
            if (info.ahead) {
                neck = info.angle;
            }
            tilt = angle / 2;
            creature->maximum_turn = WORKER_2_RUN_TURN;
            if (creature->mood == MOOD_ESCAPE) {
            } else if (Creature_CanTargetEnemy(item, &info)) {
                item->goal_anim_state = WORKER_2_STATE_WALK;
            } else if (
                creature->mood == MOOD_BORED || creature->mood == MOOD_STALK) {
                item->goal_anim_state = WORKER_2_STATE_WALK;
            }
            break;

        case WORKER_2_STATE_AIM_1:
        case WORKER_2_STATE_AIM_3:
            creature->flags = 0;
            if (info.ahead) {
                head = info.angle;
                if (!Creature_CanTargetEnemy(item, &info)) {
                    item->goal_anim_state = WORKER_2_STATE_STOP;
                } else if (item->current_anim_state == WORKER_2_STATE_AIM_1) {
                    item->goal_anim_state = WORKER_2_STATE_SHOOT_1;
                } else {
                    item->goal_anim_state = WORKER_2_STATE_SHOOT_3;
                }
            }
            break;

        case WORKER_2_STATE_AIM_2:
            creature->flags = 0;
            if (info.ahead) {
                head = info.angle;
                if (Creature_CanTargetEnemy(item, &info)) {
                    item->goal_anim_state = WORKER_2_STATE_SHOOT_2;
                } else {
                    item->goal_anim_state = WORKER_2_STATE_WALK;
                }
            }
            break;

        case WORKER_2_STATE_SHOOT_1:
        case WORKER_2_STATE_SHOOT_2:
            if (info.ahead) {
                head = info.angle;
            }
            M_ShootAtLara(item, creature, &info, head);
            break;

        case WORKER_2_STATE_SHOOT_3:
            if (item->goal_anim_state != WORKER_2_STATE_STOP) {
                if (creature->mood == MOOD_ESCAPE
                    || info.distance > WORKER_2_SHOOT_1_RANGE
                    || !Creature_CanTargetEnemy(item, &info)) {
                    item->goal_anim_state = WORKER_2_STATE_STOP;
                }
            }
            if (info.ahead) {
                head = info.angle;
            }
            M_ShootAtLara(item, creature, &info, head);
            break;

        default:
            break;
        }
    }

    Creature_Tilt(item, tilt);
    Creature_Head(item, head);
    Creature_Neck(item, neck);
    Creature_Animate(item_num, angle, 0);
}

static void M_Setup(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }

    obj->control_func = M_Control;
    obj->collision_func = Creature_Collision;

    obj->hit_points = WORKER_2_HITPOINTS;
    obj->radius = WORKER_RADIUS;
    obj->shadow_size = UNIT_SHADOW / 2;
    obj->pivot_length = 0;

    obj->intelligent = true;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_flags = true;
    obj->save_anim = true;

    Object_GetBone(obj, 4)->rot.y = true;
    Object_GetBone(obj, 13)->rot.y = true;
}

static void M_Setup5(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }

    obj->control_func = M_Control;
    obj->collision_func = Creature_Collision;

    obj->hit_points = WORKER_5_HITPOINTS;
    obj->radius = WORKER_RADIUS;
    obj->shadow_size = UNIT_SHADOW / 2;
    obj->pivot_length = 0;

    obj->intelligent = true;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_flags = true;
    obj->save_anim = true;

    Object_GetBone(obj, 4)->rot.y = true;
    Object_GetBone(obj, 13)->rot.y = true;
}

REGISTER_OBJECT(O_WORKER_2, M_Setup)
REGISTER_OBJECT(O_WORKER_5, M_Setup5)
