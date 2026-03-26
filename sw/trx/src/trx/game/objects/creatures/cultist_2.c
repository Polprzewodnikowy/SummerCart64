#include <trx/core/utils.h>
#include <trx/game/creature.h>
#include <trx/game/objects.h>
#include <trx/game/objects/creatures/cultist_common.h>
#include <trx/game/random.h>
#include <trx/game/spawn.h>

// clang-format off
#define CULTIST_2_HITPOINTS   60
#define CULTIST_2_WALK_TURN   (DEG_1 * 3) // = 546
#define CULTIST_2_RUN_TURN    (DEG_1 * 6) // = 1092
#define CULTIST_2_WALK_RANGE  SQUARE(WALL_L * 4) // = 16777216
#define CULTIST_2_KNIFE_RANGE SQUARE(WALL_L * 6) // = 37748736
#define CULTIST_2_STOP_RANGE  SQUARE(WALL_L * 5 / 2) // = 6553600
// clang-format on

typedef enum {
    // clang-format off
    CULTIST_2_STATE_EMPTY     = 0,
    CULTIST_2_STATE_STOP      = 1,
    CULTIST_2_STATE_WALK      = 2,
    CULTIST_2_STATE_RUN       = 3,
    CULTIST_2_STATE_AIM_1_L   = 4,
    CULTIST_2_STATE_SHOOT_1_L = 5,
    CULTIST_2_STATE_AIM_1_R   = 6,
    CULTIST_2_STATE_SHOOT_1_R = 7,
    CULTIST_2_STATE_AIM_2     = 8,
    CULTIST_2_STATE_SHOOT_2   = 9,
    CULTIST_2_STATE_DEATH     = 10,
    // clang-format on
} CULTIST_2_STATE;

typedef enum {
    CULTIST_2_ANIM_DEATH = 23,
} CULTIST_2_ANIM;

static const BITE m_Cultist2LeftHand = {
    .pos = { .x = 0, .y = 0, .z = 0 },
    .mesh_num = 5,
};

static const BITE m_Cultist2RightHand = {
    .pos = { .x = 0, .y = 0, .z = 0 },
    .mesh_num = 8,
};

static void M_Control(const int16_t item_num)
{
    if (!Creature_Activate(item_num)) {
        return;
    }

    ITEM *const item = Item_Get(item_num);
    CREATURE *const creature = item->creature_data;

    int16_t angle = 0;
    int16_t tilt = 0;
    int16_t neck = 0;
    int16_t head = 0;

    if (item->hit_points <= 0) {
        if (item->current_anim_state != CULTIST_2_STATE_DEATH) {
            Item_SwitchToAnim(item, CULTIST_2_ANIM_DEATH, 0);
            item->current_anim_state = CULTIST_2_STATE_DEATH;
        }
    } else {
        AI_INFO info;
        Creature_AIInfo(item, &info);
        Creature_Mood(item, &info, false);

        angle = Creature_Turn(item, creature->maximum_turn);

        switch (item->current_anim_state) {
        case CULTIST_2_STATE_STOP:
            creature->maximum_turn = 0;
            if (info.ahead) {
                neck = info.angle;
            }
            if (creature->mood == MOOD_ESCAPE) {
                item->goal_anim_state = CULTIST_2_STATE_RUN;
            } else if (Creature_CanTargetEnemy(item, &info)) {
                item->goal_anim_state = CULTIST_2_STATE_AIM_2;
            } else if (creature->mood == MOOD_BORED) {
                if (!info.ahead || info.distance > CULTIST_2_KNIFE_RANGE) {
                    item->goal_anim_state = CULTIST_2_STATE_WALK;
                }
            } else if (info.ahead && info.distance < CULTIST_2_WALK_RANGE) {
                item->goal_anim_state = CULTIST_2_STATE_WALK;
            } else {
                item->goal_anim_state = CULTIST_2_STATE_RUN;
            }
            break;

        case CULTIST_2_STATE_WALK:
            creature->maximum_turn = CULTIST_2_WALK_TURN;
            if (info.ahead) {
                neck = info.angle;
            }
            if (creature->mood == MOOD_ESCAPE) {
                item->goal_anim_state = CULTIST_2_STATE_RUN;
            } else if (Creature_CanTargetEnemy(item, &info)) {
                if (info.distance < CULTIST_2_STOP_RANGE
                    || info.zone_num != info.enemy_zone_num) {
                    item->goal_anim_state = CULTIST_2_STATE_STOP;
                } else if (Random_GetControl() < 0x4000) {
                    item->goal_anim_state = CULTIST_2_STATE_AIM_1_L;
                } else {
                    item->goal_anim_state = CULTIST_2_STATE_AIM_1_R;
                }
            } else if (creature->mood == MOOD_BORED) {
                if (info.ahead && info.distance < CULTIST_2_KNIFE_RANGE) {
                    item->goal_anim_state = CULTIST_2_STATE_STOP;
                }
            } else if (!info.ahead || info.distance > CULTIST_2_WALK_RANGE) {
                item->goal_anim_state = CULTIST_2_STATE_RUN;
            }
            break;

        case CULTIST_2_STATE_RUN:
            creature->maximum_turn = CULTIST_2_RUN_TURN;
            tilt = angle / 4;
            if (info.ahead) {
                neck = info.angle;
            }
            if (creature->mood == MOOD_ESCAPE) {
            } else if (Creature_CanTargetEnemy(item, &info)) {
                item->goal_anim_state = CULTIST_2_STATE_WALK;
            } else if (creature->mood == MOOD_BORED) {
                if (info.ahead && info.distance < CULTIST_2_KNIFE_RANGE) {
                    item->goal_anim_state = CULTIST_2_STATE_STOP;
                } else {
                    item->goal_anim_state = CULTIST_2_STATE_WALK;
                }
            } else if (info.ahead && info.distance < CULTIST_2_WALK_RANGE) {
                item->goal_anim_state = CULTIST_2_STATE_WALK;
            }
            break;

        case CULTIST_2_STATE_AIM_1_L:
            creature->flags = 0;
            if (info.ahead) {
                head = info.angle;
            }
            if (Creature_CanTargetEnemy(item, &info)) {
                item->goal_anim_state = CULTIST_2_STATE_SHOOT_1_L;
            } else {
                item->goal_anim_state = CULTIST_2_STATE_WALK;
            }
            break;

        case CULTIST_2_STATE_AIM_1_R:
            creature->flags = 0;
            if (info.ahead) {
                head = info.angle;
            }
            if (Creature_CanTargetEnemy(item, &info)) {
                item->goal_anim_state = CULTIST_2_STATE_SHOOT_1_R;
            } else {
                item->goal_anim_state = CULTIST_2_STATE_WALK;
            }
            break;

        case CULTIST_2_STATE_AIM_2:
            creature->flags = 0;
            if (info.ahead) {
                head = info.angle;
            }
            if (Creature_CanTargetEnemy(item, &info)) {
                item->goal_anim_state = CULTIST_2_STATE_SHOOT_2;
            } else {
                item->goal_anim_state = CULTIST_2_STATE_STOP;
            }
            break;

        case CULTIST_2_STATE_SHOOT_1_L:
            if (info.ahead) {
                head = info.angle;
            }
            if (creature->flags == 0) {
                Creature_Effect(item, &m_Cultist2LeftHand, Spawn_Knife);
                creature->flags = 1;
            }
            break;

        case CULTIST_2_STATE_SHOOT_1_R:
            if (info.ahead) {
                head = info.angle;
            }
            if (creature->flags == 0) {
                Creature_Effect(item, &m_Cultist2RightHand, Spawn_Knife);
                creature->flags = 1;
            }
            break;

        case CULTIST_2_STATE_SHOOT_2:
            if (info.ahead) {
                head = info.angle;
            }
            if (creature->flags == 0) {
                Creature_Effect(item, &m_Cultist2LeftHand, Spawn_Knife);
                Creature_Effect(item, &m_Cultist2RightHand, Spawn_Knife);
                creature->flags = CULTIST_2_STATE_STOP;
            }
            break;

        default:
            break;
        }
    }

    Creature_Tilt(item, tilt);
    Creature_Neck(item, neck);
    Creature_Head(item, head);
    Creature_Animate(item_num, angle, 0);
}

static void M_Setup(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }

    obj->control_func = M_Control;
    obj->collision_func = Creature_Collision;

    obj->hit_points = CULTIST_2_HITPOINTS;
    obj->radius = CULTIST_RADIUS;
    obj->shadow_size = UNIT_SHADOW / 2;
    obj->pivot_length = 50;

    obj->intelligent = true;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_flags = true;
    obj->save_anim = true;

    Object_GetBone(obj, 0)->rot.y = true;
    Object_GetBone(obj, 8)->rot.y = true;
}

REGISTER_OBJECT(O_CULT_2, M_Setup)
