#include <trx/core/utils.h>
#include <trx/game/creature.h>
#include <trx/game/lara.h>
#include <trx/game/objects/common.h>
#include <trx/game/random.h>
#include <trx/game/spawn.h>

// clang-format off
#define M_HITPOINTS     4
#define M_TOUCH_BITS    0b01111111 // = 0x7F
#define M_RADIUS        (WALL_L / 10) // = 102
#define M_RUN_TURN      (DEG_1 * 6) // = 1092
#define M_ATTACK_RANGE  SQUARE(WALL_L / 3) // = 116281
#define M_BITE_DAMAGE   20
#define M_WAIT_1_CHANCE 0x500 // = 1280
#define M_WAIT_2_CHANCE (M_WAIT_1_CHANCE + 0x500) // = 2560
// clang-format on

typedef enum {
    M_STATE_NULL,
    M_STATE_RUN,
    M_STATE_STOP,
    M_STATE_WAIT_1,
    M_STATE_WAIT_2,
    M_STATE_ATTACK,
    M_STATE_DEATH,
} M_STATE;

typedef enum {
    M_ANIM_DEATH = 9,
} M_ANIM;

static const BITE m_MouseBite = {
    .pos = { .x = 0, .y = 0, .z = 57 },
    .mesh_num = 2,
};

static void M_Control(const int16_t item_num)
{
    if (!Creature_Activate(item_num)) {
        return;
    }

    ITEM *const item = Item_Get(item_num);
    CREATURE *const creature = item->creature_data;

    int16_t head = 0;
    int16_t angle = 0;

    if (item->hit_points <= 0) {
        if (item->current_anim_state != M_STATE_DEATH) {
            Item_SwitchToAnim(item, M_ANIM_DEATH, 0);
            item->current_anim_state = M_STATE_DEATH;
        }
    } else {
        AI_INFO info;
        Creature_AIInfo(item, &info);
        Creature_Mood(item, &info, false);

        if (info.ahead) {
            head = info.angle;
        }
        angle = Creature_Turn(item, M_RUN_TURN);

        switch (item->current_anim_state) {
        case M_STATE_RUN:
            creature->maximum_turn = M_RUN_TURN;
            if (creature->mood == MOOD_BORED || creature->mood == MOOD_STALK) {
                const int32_t random = Random_GetControl();
                if (random < M_WAIT_1_CHANCE) {
                    item->required_anim_state = M_STATE_WAIT_1;
                    item->goal_anim_state = M_STATE_STOP;
                } else if (random < M_WAIT_2_CHANCE) {
                    item->goal_anim_state = M_STATE_STOP;
                }
            } else if (info.ahead && info.distance < M_ATTACK_RANGE) {
                item->goal_anim_state = M_STATE_STOP;
            }
            break;

        case M_STATE_STOP:
            creature->maximum_turn = 0;
            if (item->required_anim_state != M_STATE_NULL) {
                item->goal_anim_state = item->required_anim_state;
            }
            break;

        case M_STATE_WAIT_1:
            if (Random_GetControl() < M_WAIT_1_CHANCE) {
                item->goal_anim_state = M_STATE_STOP;
            }
            break;

        case M_STATE_WAIT_2:
            if (creature->mood == MOOD_BORED || creature->mood == MOOD_STALK) {
                const int32_t random = Random_GetControl();
                if (random < M_WAIT_1_CHANCE) {
                    item->required_anim_state = M_STATE_WAIT_1;
                } else if (random > M_WAIT_2_CHANCE) {
                    item->required_anim_state = M_STATE_RUN;
                }
            } else if (info.distance < M_ATTACK_RANGE) {
                item->required_anim_state = M_STATE_ATTACK;
            } else {
                item->required_anim_state = M_STATE_RUN;
            }

            if (item->required_anim_state != M_STATE_NULL) {
                item->goal_anim_state = M_STATE_STOP;
            }
            break;

        case M_STATE_ATTACK:
            if (item->required_anim_state == M_STATE_NULL
                && (item->touch_bits & M_TOUCH_BITS) != 0) {
                Lara_TakeDamage(M_BITE_DAMAGE, true);
                Creature_Effect(item, &m_MouseBite, Spawn_Blood);
                item->required_anim_state = M_STATE_STOP;
            }
            break;

        default:
            break;
        }
    }

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

    obj->hit_points = M_HITPOINTS;
    obj->radius = M_RADIUS;
    obj->shadow_size = UNIT_SHADOW / 2;
    obj->pivot_length = 50;

    obj->intelligent = true;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_flags = true;
    obj->save_anim = true;

    Object_GetBone(obj, 3)->rot.y = true;
}

REGISTER_OBJECT(O_MOUSE, M_Setup)
