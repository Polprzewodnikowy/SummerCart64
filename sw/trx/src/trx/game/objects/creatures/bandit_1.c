#include <trx/core/utils.h>
#include <trx/debug.h>
#include <trx/game/creature.h>
#include <trx/game/objects/common.h>
#include <trx/game/objects/creatures/bandit_common.h>
#include <trx/game/random.h>
#include <trx/game/rooms.h>
#include <trx/game/spawn.h>

// clang-format off
#define BANDIT_1_HITPOINTS      45
#define BANDIT_1_SHOOT_DAMAGE   8
#define BANDIT_1_WALK_TURN      (DEG_1 * 4) // = 728
#define BANDIT_1_RUN_TURN       (DEG_1 * 6) // = 1092
#define BANDIT_1_WALK_RANGE     SQUARE(WALL_L * 2) // = 4194304
#define BANDIT_1_SHOOT_1_CHANCE 0x2000
#define BANDIT_1_SHOOT_2_CHANCE 0x4000
// clang-format on

typedef enum {
    // clang-format off
    BANDIT_1_STATE_EMPTY    = 0,
    BANDIT_1_STATE_WAIT     = 1,
    BANDIT_1_STATE_WALK     = 2,
    BANDIT_1_STATE_RUN      = 3,
    BANDIT_1_STATE_AIM_1    = 4,
    BANDIT_1_STATE_SHOOT_1  = 5,
    BANDIT_1_STATE_AIM_2    = 6,
    BANDIT_1_STATE_SHOOT_2  = 7,
    BANDIT_1_STATE_SHOOT_3A = 8,
    BANDIT_1_STATE_SHOOT_3B = 9,
    BANDIT_1_STATE_SHOOT_4A = 10,
    BANDIT_1_STATE_AIM_3    = 11,
    BANDIT_1_STATE_AIM_4    = 12,
    BANDIT_1_STATE_DEATH    = 13,
    BANDIT_1_STATE_SHOOT_4B = 14,
    // clang-format on
} BANDIT_1_STATE;

typedef enum {
    BANDIT_1_ANIM_DEATH = 14,
} BANDIT_1_ANIM;

static const CREATURE_GUN m_Bandit1Gun = {
    .muzzle = { .pos = { .x = -2, .y = 150, .z = 19 }, .mesh_num = 17 },
};

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
        item->hit_points = 0;
        if (item->current_anim_state != BANDIT_1_STATE_DEATH) {
            Item_SwitchToAnim(item, BANDIT_1_ANIM_DEATH, 0);
            item->current_anim_state = BANDIT_1_STATE_DEATH;
        }
    } else {
        AI_INFO info;
        Creature_AIInfo(item, &info);
        Creature_Mood(item, &info, false);

        angle = Creature_Turn(item, creature->maximum_turn);

        switch (item->current_anim_state) {
        case BANDIT_1_STATE_WAIT:
            if (info.ahead) {
                neck = info.angle;
            }
            creature->maximum_turn = 0;
            if (creature->mood == MOOD_ESCAPE) {
                item->goal_anim_state = BANDIT_1_STATE_RUN;
            } else if (Creature_CanTargetEnemy(item, &info)) {
                if (info.distance > BANDIT_1_WALK_RANGE) {
                    item->goal_anim_state = BANDIT_1_STATE_WALK;
                } else {
                    const int32_t random = Random_GetControl();
                    if (random < BANDIT_1_SHOOT_1_CHANCE) {
                        item->goal_anim_state = BANDIT_1_STATE_SHOOT_1;
                    } else if (random < BANDIT_1_SHOOT_2_CHANCE) {
                        item->goal_anim_state = BANDIT_1_STATE_SHOOT_2;
                    } else {
                        item->goal_anim_state = BANDIT_1_STATE_AIM_3;
                    }
                }
            } else if (creature->mood == MOOD_BORED) {
                if (info.ahead) {
                    item->goal_anim_state = BANDIT_1_STATE_WAIT;
                } else {
                    item->goal_anim_state = BANDIT_1_STATE_WALK;
                }
            } else {
                item->goal_anim_state = BANDIT_1_STATE_RUN;
            }
            break;

        case BANDIT_1_STATE_WALK:
            if (info.ahead) {
                neck = info.angle;
            }
            creature->maximum_turn = BANDIT_1_WALK_TURN;
            if (creature->mood == MOOD_ESCAPE) {
                item->goal_anim_state = BANDIT_1_STATE_RUN;
            } else if (Creature_CanTargetEnemy(item, &info)) {
                if (info.distance > BANDIT_1_WALK_RANGE
                    && info.zone_num == info.enemy_zone_num) {
                    item->goal_anim_state = BANDIT_1_STATE_AIM_4;
                } else {
                    item->goal_anim_state = BANDIT_1_STATE_WAIT;
                }
            } else if (creature->mood == MOOD_BORED) {
                if (info.ahead) {
                    item->goal_anim_state = BANDIT_1_STATE_WALK;
                } else {
                    item->goal_anim_state = BANDIT_1_STATE_WAIT;
                }
            } else {
                item->goal_anim_state = BANDIT_1_STATE_RUN;
            }
            break;

        case BANDIT_1_STATE_RUN:
            if (info.ahead) {
                neck = info.angle;
            }
            tilt = angle / 2;
            creature->maximum_turn = BANDIT_1_RUN_TURN;
            if (creature->mood == MOOD_ESCAPE) {
            } else if (Creature_CanTargetEnemy(item, &info)) {
                item->goal_anim_state = BANDIT_1_STATE_WAIT;
            } else if (creature->mood == MOOD_BORED) {
                item->goal_anim_state = BANDIT_1_STATE_WALK;
            }
            break;

        case BANDIT_1_STATE_SHOOT_1:
        case BANDIT_1_STATE_SHOOT_2:
        case BANDIT_1_STATE_SHOOT_3A:
        case BANDIT_1_STATE_SHOOT_3B:
            if (info.ahead) {
                head = info.angle;
            }
            if (!Creature_Shoot(
                    item, &info, &m_Bandit1Gun, head, BANDIT_1_SHOOT_DAMAGE)) {
                item->goal_anim_state = BANDIT_1_STATE_WAIT;
            }
            break;

        case BANDIT_1_STATE_SHOOT_4A:
        case BANDIT_1_STATE_SHOOT_4B:
            if (info.ahead) {
                head = info.angle;
            }
            if (!Creature_Shoot(
                    item, &info, &m_Bandit1Gun, head, BANDIT_1_SHOOT_DAMAGE)) {
                item->goal_anim_state = BANDIT_1_STATE_WALK;
            }
            if (info.distance < BANDIT_1_WALK_RANGE) {
                item->goal_anim_state = BANDIT_1_STATE_WALK;
            }
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

    obj->hit_points = BANDIT_1_HITPOINTS;
    obj->radius = BANDIT_RADIUS;
    obj->shadow_size = UNIT_SHADOW / 2;
    obj->pivot_length = 0;

    obj->intelligent = true;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_flags = true;
    obj->save_anim = true;

    Object_GetBone(obj, 6)->rot.y = true;
    Object_GetBone(obj, 8)->rot.y = true;
}

REGISTER_OBJECT(O_BANDIT_1, M_Setup)
