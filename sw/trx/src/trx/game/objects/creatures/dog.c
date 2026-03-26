#include <trx/core/utils.h>
#include <trx/game/creature.h>
#include <trx/game/lara.h>
#include <trx/game/objects/common.h>
#include <trx/game/random.h>
#include <trx/game/spawn.h>

// clang-format off
#define M_HITPOINTS       10
#define M_TOUCH_BITS      0b00000001'00111111'01110000'00000000
#define M_RADIUS          (WALL_L / 3) // = 341
#define M_WALK_TURN       (3 * DEG_1) // = 546
#define M_RUN_TURN        (6 * DEG_1) // = 1092
#define M_ATTACK_1_RANGE  SQUARE(WALL_L / 3) // = 116281
#define M_ATTACK_2_RANGE  SQUARE(WALL_L * 3 / 4) // = 589824
#define M_ATTACK_3_RANGE  SQUARE(WALL_L * 2 / 3) // = 465124
#define M_LEAP_DAMAGE     200
#define M_BITE_DAMAGE     100
#define M_LUNGE_DAMAGE    100
#define M_BARK_CHANCE     0x300
#define M_CROUCH_CHANCE   (M_BARK_CHANCE + 0x300) // = 0x600
#define M_STAND_CHANCE    (M_CROUCH_CHANCE + 0x500) // = 0xB00
#define M_WALK_CHANCE     (M_STAND_CHANCE + 0x2000) // = 0x2600
#define M_UNCROUCH_CHANCE 0x100
#define M_UNSTAND_CHANCE  0x200
#define M_UNBARK_CHANCE   0x500
// clang-format on

typedef enum {
    M_STATE_EMPTY,
    M_STATE_WALK,
    M_STATE_RUN,
    M_STATE_STOP,
    M_STATE_BARK,
    M_STATE_CROUCH,
    M_STATE_STAND,
    M_STATE_ATTACK_1,
    M_STATE_ATTACK_2,
    M_STATE_ATTACK_3,
    M_STATE_DEATH,
} M_STATE;

typedef enum {
    M_ANIM_DEATH = 13,
} M_ANIM;

static const BITE m_DogBite = {
    .pos = { .x = 0, .y = 30, .z = 141 },
    .mesh_num = 20,
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
    int16_t tilt = 0;

    if (item->hit_points > 0) {
        AI_INFO info;
        Creature_AIInfo(item, &info);
        Creature_Mood(item, &info, false);

        if (info.ahead) {
            head = info.angle;
        }
        angle = Creature_Turn(item, creature->maximum_turn);

        switch (item->current_anim_state) {
        case M_STATE_WALK:
            creature->maximum_turn = M_WALK_TURN;
            if (creature->mood == MOOD_BORED) {
                const int16_t random = Random_GetControl();
                if (random < M_BARK_CHANCE) {
                    item->required_anim_state = M_STATE_BARK;
                    item->goal_anim_state = M_STATE_STOP;
                } else if (random < M_CROUCH_CHANCE) {
                    item->required_anim_state = M_STATE_CROUCH;
                    item->goal_anim_state = M_STATE_STOP;
                } else if (random < M_STAND_CHANCE) {
                    item->goal_anim_state = M_STATE_STOP;
                }
            } else {
                item->goal_anim_state = M_STATE_RUN;
            }
            break;

        case M_STATE_RUN:
            tilt = angle;
            creature->maximum_turn = M_RUN_TURN;
            if (creature->mood == MOOD_BORED) {
                item->goal_anim_state = M_STATE_STOP;
            } else if (info.distance < M_ATTACK_2_RANGE) {
                item->goal_anim_state = M_STATE_ATTACK_2;
            }
            break;

        case M_STATE_STOP:
            creature->maximum_turn = 0;
            creature->flags = 0;
            if (creature->mood != MOOD_BORED) {
                if (creature->mood == MOOD_ESCAPE) {
                    item->goal_anim_state = M_STATE_RUN;
                } else if (info.distance < M_ATTACK_1_RANGE && info.ahead) {
                    item->goal_anim_state = M_STATE_ATTACK_1;
                } else {
                    item->goal_anim_state = M_STATE_RUN;
                }
            } else if (item->required_anim_state != M_STATE_EMPTY) {
                item->goal_anim_state = item->required_anim_state;
            } else {
                const int16_t random = Random_GetControl();
                if (random < M_BARK_CHANCE) {
                    item->goal_anim_state = M_STATE_BARK;
                } else if (random < M_CROUCH_CHANCE) {
                    item->goal_anim_state = M_STATE_CROUCH;
                } else if (random < M_WALK_CHANCE) {
                    item->goal_anim_state = M_STATE_WALK;
                }
            }
            break;

        case M_STATE_BARK:
            if (creature->mood || Random_GetControl() < M_UNBARK_CHANCE) {
                item->goal_anim_state = M_STATE_STOP;
            }
            break;

        case M_STATE_CROUCH:
            if (creature->mood || Random_GetControl() < M_UNCROUCH_CHANCE) {
                item->goal_anim_state = M_STATE_STOP;
            }
            break;

        case M_STATE_STAND:
            if (creature->mood || Random_GetControl() < M_UNSTAND_CHANCE) {
                item->goal_anim_state = M_STATE_STOP;
            }
            break;

        case M_STATE_ATTACK_1:
            creature->maximum_turn = 0;
            if (creature->flags != 1 && info.ahead
                && (item->touch_bits & M_TOUCH_BITS) != 0) {
                Creature_Effect(item, &m_DogBite, Spawn_Blood);
                Lara_TakeDamage(M_BITE_DAMAGE, true);
                creature->flags = 1;
            }
            if (info.distance > M_ATTACK_1_RANGE
                && info.distance < M_ATTACK_3_RANGE) {
                item->goal_anim_state = M_STATE_ATTACK_3;
            } else {
                item->goal_anim_state = M_STATE_STOP;
            }
            break;

        case M_STATE_ATTACK_2:
            if (creature->flags != 2
                && (item->touch_bits & M_TOUCH_BITS) != 0) {
                Creature_Effect(item, &m_DogBite, Spawn_Blood);
                Lara_TakeDamage(M_LUNGE_DAMAGE, true);
                creature->flags = 2;
            }
            if (info.distance < M_ATTACK_1_RANGE) {
                item->goal_anim_state = M_STATE_ATTACK_1;
            } else if (info.distance < M_ATTACK_3_RANGE) {
                item->goal_anim_state = M_STATE_ATTACK_3;
            }
            break;

        case M_STATE_ATTACK_3:
            creature->maximum_turn = M_RUN_TURN;
            if (creature->flags != 3
                && (item->touch_bits & M_TOUCH_BITS) != 0) {
                Creature_Effect(item, &m_DogBite, Spawn_Blood);
                Lara_TakeDamage(M_LUNGE_DAMAGE, true);
                creature->flags = 3;
            }
            if (info.distance < M_ATTACK_1_RANGE) {
                item->goal_anim_state = M_STATE_ATTACK_1;
            }
            break;

        default:
            break;
        }
    } else if (item->current_anim_state != M_STATE_DEATH) {
        Item_SwitchToAnim(item, M_ANIM_DEATH, 0);
        item->current_anim_state = M_STATE_DEATH;
    }

    Creature_Tilt(item, tilt);
    Creature_Head(item, head);
    Creature_Animate(item_num, angle, tilt);
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
    obj->pivot_length = 300;

    obj->intelligent = true;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_flags = true;
    obj->save_anim = true;

    Object_GetBone(obj, 19)->rot.y = true;
}

REGISTER_OBJECT(O_DOG, M_Setup)
