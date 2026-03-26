#include <trx/config.h>
#include <trx/core/utils.h>
#include <trx/game/creature.h>
#include <trx/game/lara.h>
#include <trx/game/objects/common.h>
#include <trx/game/random.h>
#include <trx/game/spawn.h>

// clang-format off
#define BIRD_GUARDIAN_HITPOINTS      200
#define BIRD_GUARDIAN_TOUCH_BITS_L   0b00001100'00000000'00000000 // = 0x0C0000
#define BIRD_GUARDIAN_TOUCH_BITS_R   0b01100000'00000000'00000000 // = 0x600000
#define BIRD_GUARDIAN_RADIUS         (WALL_L / 3) // = 341
#define BIRD_GUARDIAN_WALK_TURN      (DEG_1 * 4) // = 728
#define BIRD_GUARDIAN_ATTACK_1_RANGE SQUARE(WALL_L) // = 1048576
#define BIRD_GUARDIAN_ATTACK_2_RANGE SQUARE(WALL_L * 2) // = 4194304
#define BIRD_GUARDIAN_PUNCH_DAMAGE   200
// clang-format on

typedef enum {
    // clang-format off
    BIRD_GUARDIAN_STATE_EMPTY   = 0,
    BIRD_GUARDIAN_STATE_WAIT    = 1,
    BIRD_GUARDIAN_STATE_WALK    = 2,
    BIRD_GUARDIAN_STATE_AIM_1   = 3,
    BIRD_GUARDIAN_STATE_PUNCH_1 = 4,
    BIRD_GUARDIAN_STATE_AIM_2   = 5,
    BIRD_GUARDIAN_STATE_PUNCH_2 = 6,
    BIRD_GUARDIAN_STATE_PUNCH_R = 7,
    BIRD_GUARDIAN_STATE_WAIT_2  = 8,
    BIRD_GUARDIAN_STATE_DEATH   = 9,
    BIRD_GUARDIAN_STATE_AIM_3   = 10,
    BIRD_GUARDIAN_STATE_PUNCH_3 = 11,
    // clang-format on
} BIRD_GUARDIAN_STATE;

typedef enum {
    BIRD_GUARDIAN_ANIM_DEATH = 20,
} BIRD_GUARDIAN_ANIM;

static const BITE m_BirdGuardianBiteL = {
    .pos = { .x = 0, .y = 224, .z = 0, },
    .mesh_num = 19,
};

static const BITE m_BirdGuardianBiteR = {
    .pos = { .x = 0, .y = 224, .z = 0, },
    .mesh_num = 22,
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

    if (item->hit_points > 0) {
        AI_INFO info;
        Creature_AIInfo(item, &info);
        Creature_Mood(item, &info, true);

        if (info.ahead) {
            head = info.angle;
        }
        angle = Creature_Turn(item, creature->maximum_turn);

        switch (item->current_anim_state) {
        case BIRD_GUARDIAN_STATE_WAIT:
            creature->maximum_turn = 0;
            if (info.ahead && info.distance < BIRD_GUARDIAN_ATTACK_1_RANGE) {
                if (Random_GetControl() < 0x4000) {
                    item->goal_anim_state = BIRD_GUARDIAN_STATE_AIM_1;
                } else {
                    item->goal_anim_state = BIRD_GUARDIAN_STATE_AIM_3;
                }
            } else if (info.ahead && creature->mood == MOOD_BORED) {
                item->goal_anim_state = BIRD_GUARDIAN_STATE_WAIT_2;
            } else if (info.ahead && creature->mood == MOOD_STALK) {
                item->goal_anim_state = BIRD_GUARDIAN_STATE_WAIT_2;
            } else {
                item->goal_anim_state = BIRD_GUARDIAN_STATE_WALK;
            }
            break;

        case BIRD_GUARDIAN_STATE_WAIT_2:
            if (creature->mood != MOOD_BORED) {
                item->goal_anim_state = BIRD_GUARDIAN_STATE_WAIT;
            } else if (!info.ahead) {
                item->goal_anim_state = BIRD_GUARDIAN_STATE_WAIT;
            }
            break;

        case BIRD_GUARDIAN_STATE_WALK:
            creature->maximum_turn = BIRD_GUARDIAN_WALK_TURN;
            if (info.ahead && info.distance < BIRD_GUARDIAN_ATTACK_2_RANGE) {
                item->goal_anim_state = BIRD_GUARDIAN_STATE_AIM_2;
            } else if (info.ahead && creature->mood == MOOD_BORED) {
                item->goal_anim_state = BIRD_GUARDIAN_STATE_WAIT;
            } else if (info.ahead && creature->mood == MOOD_STALK) {
                item->goal_anim_state = BIRD_GUARDIAN_STATE_WAIT;
            }
            break;

        case BIRD_GUARDIAN_STATE_AIM_1:
            creature->flags = 0;
            if (info.ahead && info.distance < BIRD_GUARDIAN_ATTACK_1_RANGE) {
                item->goal_anim_state = BIRD_GUARDIAN_STATE_PUNCH_1;
            } else {
                item->goal_anim_state = BIRD_GUARDIAN_STATE_WAIT;
            }
            break;

        case BIRD_GUARDIAN_STATE_AIM_2:
            creature->flags = 0;
            if (info.ahead && info.distance < BIRD_GUARDIAN_ATTACK_2_RANGE) {
                item->goal_anim_state = BIRD_GUARDIAN_STATE_PUNCH_2;
            } else {
                item->goal_anim_state = BIRD_GUARDIAN_STATE_WALK;
            }
            break;

        case BIRD_GUARDIAN_STATE_AIM_3:
            creature->flags = 0;
            if (info.ahead && info.distance < BIRD_GUARDIAN_ATTACK_1_RANGE) {
                item->goal_anim_state = BIRD_GUARDIAN_STATE_PUNCH_3;
            } else {
                item->goal_anim_state = BIRD_GUARDIAN_STATE_WAIT;
            }
            break;

        case BIRD_GUARDIAN_STATE_PUNCH_1:
        case BIRD_GUARDIAN_STATE_PUNCH_2:
        case BIRD_GUARDIAN_STATE_PUNCH_R:
        case BIRD_GUARDIAN_STATE_PUNCH_3:
            if ((creature->flags & 1) == 0
                && (item->touch_bits & BIRD_GUARDIAN_TOUCH_BITS_R) != 0) {
                Creature_Effect(item, &m_BirdGuardianBiteR, Spawn_Blood);
                Lara_TakeDamage(BIRD_GUARDIAN_PUNCH_DAMAGE, true);
                creature->flags |= 1;
            }
            if ((creature->flags & 2) == 0
                && (item->touch_bits & BIRD_GUARDIAN_TOUCH_BITS_L) != 0) {
                Creature_Effect(item, &m_BirdGuardianBiteL, Spawn_Blood);
                Lara_TakeDamage(BIRD_GUARDIAN_PUNCH_DAMAGE, true);
                creature->flags |= 2;
            }
            break;

        default:
            break;
        }
    } else if (item->current_anim_state != BIRD_GUARDIAN_STATE_DEATH) {
        Item_SwitchToAnim(item, BIRD_GUARDIAN_ANIM_DEATH, 0);
        item->current_anim_state = BIRD_GUARDIAN_STATE_DEATH;
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

    obj->hit_points = BIRD_GUARDIAN_HITPOINTS;
    obj->radius = BIRD_GUARDIAN_RADIUS;
    if (g_Config.visuals.fix_texture_issues) {
        obj->shadow_size = UNIT_SHADOW / 2;
    }

    obj->intelligent = true;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_flags = true;
    obj->save_anim = true;

    Object_GetBone(obj, 14)->rot.y = true;
}

REGISTER_OBJECT(O_BIRD_GUARDIAN, M_Setup)
