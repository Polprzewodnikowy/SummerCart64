#include <trx/core/utils.h>
#include <trx/game/creature.h>
#include <trx/game/lara.h>
#include <trx/game/objects.h>
#include <trx/game/objects/creatures/worker_common.h>
#include <trx/game/pathing.h>
#include <trx/game/random.h>
#include <trx/game/sound.h>
#include <trx/game/spawn.h>

// clang-format off
#define WORKER_3_HITPOINTS      27
#define WORKER_4_HITPOINTS      27
#define WORKER_3_HIT_DAMAGE     80
#define WORKER_3_SWIPE_DAMAGE   100
#define WORKER_3_WALK_TURN      (DEG_1 * 5) // = 910
#define WORKER_3_RUN_TURN       (DEG_1 * 6) // = 1092
#define WORKER_3_ATTACK_0_RANGE SQUARE(WALL_L / 2) // = 262144 = 0x40000
#define WORKER_3_ATTACK_1_RANGE SQUARE(WALL_L) // = 1048576 = 0x100000
#define WORKER_3_ATTACK_2_RANGE SQUARE(WALL_L * 3 / 2) // = WORKER_3_ATTACK_2_RANGE
#define WORKER_3_WALK_RANGE     SQUARE(WALL_L * 2) // = 4194304 = 0x400000
#define WORKER_3_WALK_CHANCE    0x100
#define WORKER_3_WAIT_CHANCE    0x100
#define WORKER_3_TOUCH_BITS     0b00000110'00000000 // = 0x600
#define WORKER_3_VAULT_SHIFT    260
// clang-format on

typedef enum {
    // clang-format off
    WORKER_3_STATE_EMPTY   = 0,
    WORKER_3_STATE_STOP    = 1,
    WORKER_3_STATE_WALK    = 2,
    WORKER_3_STATE_PUNCH_2 = 3,
    WORKER_3_STATE_AIM_2   = 4,
    WORKER_3_STATE_WAIT    = 5,
    WORKER_3_STATE_AIM_1   = 6,
    WORKER_3_STATE_AIM_0   = 7,
    WORKER_3_STATE_PUNCH_1 = 8,
    WORKER_3_STATE_PUNCH_0 = 9,
    WORKER_3_STATE_RUN     = 10,
    WORKER_3_STATE_DEATH   = 11,
    WORKER_3_STATE_CLIMB_3 = 12,
    WORKER_3_STATE_CLIMB_1 = 13,
    WORKER_3_STATE_CLIMB_2 = 14,
    WORKER_3_STATE_FALL_3  = 15,
    // clang-format on
} WORKER_3_STATE;

typedef enum {
    // clang-format off
    WORKER_3_ANIM_DEATH   = 26,
    WORKER_3_ANIM_CLIMB_1 = 28,
    WORKER_3_ANIM_CLIMB_2 = 29,
    WORKER_3_ANIM_CLIMB_3 = 27,
    WORKER_3_ANIM_FALL_3  = 30,
    // clang-format on
} WORKER_3_ANIM;

static const BITE m_Worker3Hit = {
    .pos = { .x = 247, .y = 10, .z = 11 },
    .mesh_num = 10,
};

static void M_Control(const int16_t item_num)
{
    if (!Creature_Activate(item_num)) {
        return;
    }

    ITEM *const item = Item_Get(item_num);
    CREATURE *const creature = item->creature_data;

    int16_t tilt = 0;
    int16_t angle = 0;
    int16_t neck = 0;
    int16_t head = 0;

    if (item->hit_points <= 0) {
        if (item->current_anim_state != WORKER_3_STATE_DEATH) {
            Item_SwitchToObjAnim(item, WORKER_3_ANIM_DEATH, 0, O_WORKER_3);
            item->current_anim_state = WORKER_3_STATE_DEATH;
        }
    } else {
        AI_INFO info;
        Creature_AIInfo(item, &info);
        Creature_Mood(item, &info, false);

        angle = Creature_Turn(item, creature->maximum_turn);

        switch (item->current_anim_state) {
        case WORKER_3_STATE_STOP:
            creature->flags = 0;
            if (info.ahead) {
                neck = info.angle;
            }
            creature->maximum_turn = 0;
            if (creature->mood == MOOD_ESCAPE) {
                item->goal_anim_state = WORKER_3_STATE_RUN;
            } else if (creature->mood == MOOD_BORED) {
                if (item->required_anim_state != WORKER_3_STATE_EMPTY) {
                    item->goal_anim_state = item->required_anim_state;
                } else if (Random_GetControl() < 0x4000) {
                    item->goal_anim_state = WORKER_3_STATE_WALK;
                } else {
                    item->goal_anim_state = WORKER_3_STATE_WAIT;
                }
            } else if (!info.bite) {
                item->goal_anim_state = WORKER_3_STATE_RUN;
            } else if (info.distance < WORKER_3_ATTACK_0_RANGE) {
                item->goal_anim_state = WORKER_3_STATE_AIM_0;
            } else if (info.distance < WORKER_3_ATTACK_1_RANGE) {
                item->goal_anim_state = WORKER_3_STATE_AIM_1;
            } else if (info.distance < WORKER_3_WALK_RANGE) {
                item->goal_anim_state = WORKER_3_STATE_WALK;
            } else {
                item->goal_anim_state = WORKER_3_STATE_RUN;
            }
            break;

        case WORKER_3_STATE_WAIT:
            if (info.ahead) {
                neck = info.angle;
            }
            if (creature->mood != MOOD_BORED) {
                item->goal_anim_state = WORKER_3_STATE_STOP;
            } else if (Random_GetControl() < WORKER_3_WALK_CHANCE) {
                item->required_anim_state = WORKER_3_STATE_WALK;
                item->goal_anim_state = WORKER_3_STATE_STOP;
            }
            break;

        case WORKER_3_STATE_WALK:
            if (info.ahead) {
                neck = info.angle;
            }
            creature->maximum_turn = WORKER_3_WALK_TURN;
            if (creature->mood == MOOD_ESCAPE) {
                item->goal_anim_state = WORKER_3_STATE_RUN;
            } else if (creature->mood == MOOD_BORED) {
                if (Random_GetControl() < WORKER_3_WAIT_CHANCE) {
                    item->required_anim_state = WORKER_3_STATE_WAIT;
                    item->goal_anim_state = WORKER_3_STATE_STOP;
                }
            } else if (!info.bite) {
                item->goal_anim_state = WORKER_3_STATE_RUN;
            } else if (info.distance < WORKER_3_ATTACK_0_RANGE) {
                item->goal_anim_state = WORKER_3_STATE_STOP;
            } else if (info.distance < WORKER_3_ATTACK_2_RANGE) {
                item->goal_anim_state = WORKER_3_STATE_AIM_2;
            } else {
                item->goal_anim_state = WORKER_3_STATE_RUN;
            }
            break;

        case WORKER_3_STATE_RUN:
            if (info.ahead) {
                neck = info.angle;
            }
            creature->maximum_turn = WORKER_3_RUN_TURN;
            tilt = angle / 2;
            if (creature->mood == MOOD_ESCAPE) {
            } else if (creature->mood == MOOD_BORED) {
                item->goal_anim_state = WORKER_3_STATE_WALK;
            } else if (info.ahead && info.distance < WORKER_3_WALK_RANGE) {
                item->goal_anim_state = WORKER_3_STATE_WALK;
            }
            break;

        case WORKER_3_STATE_AIM_0:
            if (info.ahead) {
                head = info.angle;
            }
            creature->flags = 0;
            if (info.bite && info.distance < WORKER_3_ATTACK_0_RANGE) {
                item->goal_anim_state = WORKER_3_STATE_PUNCH_0;
            } else {
                item->goal_anim_state = WORKER_3_STATE_STOP;
            }
            break;

        case WORKER_3_STATE_AIM_1:
            if (info.ahead) {
                head = info.angle;
            }
            creature->flags = 0;
            if (info.ahead && info.distance < WORKER_3_ATTACK_1_RANGE) {
                item->goal_anim_state = WORKER_3_STATE_PUNCH_1;
            } else {
                item->goal_anim_state = WORKER_3_STATE_STOP;
            }
            break;

        case WORKER_3_STATE_AIM_2:
            if (info.ahead) {
                head = info.angle;
            }
            creature->flags = 0;
            if (info.bite && info.distance < WORKER_3_ATTACK_2_RANGE) {
                item->goal_anim_state = WORKER_3_STATE_PUNCH_2;
            } else {
                item->goal_anim_state = WORKER_3_STATE_WALK;
            }
            break;

        case WORKER_3_STATE_PUNCH_0:
            if (info.ahead) {
                head = info.angle;
            }
            if (creature->flags == 0
                && (item->touch_bits & WORKER_3_TOUCH_BITS) != 0) {
                Lara_TakeDamage(WORKER_3_HIT_DAMAGE, true);
                Creature_Effect(item, &m_Worker3Hit, Spawn_Blood);
                Sound_Effect(SFX_ENEMY_HIT_2, &item->pos, SPM_NORMAL);
                creature->flags = 1;
            }
            break;

        case WORKER_3_STATE_PUNCH_1:
            if (info.ahead) {
                head = info.angle;
            }
            if (creature->flags == 0
                && (item->touch_bits & WORKER_3_TOUCH_BITS) != 0) {
                Lara_TakeDamage(WORKER_3_HIT_DAMAGE, true);
                Creature_Effect(item, &m_Worker3Hit, Spawn_Blood);
                Sound_Effect(SFX_ENEMY_HIT_1, &item->pos, SPM_NORMAL);
                creature->flags = 1;
            }
            if (info.ahead && info.distance > WORKER_3_ATTACK_1_RANGE
                && info.distance < WORKER_3_ATTACK_2_RANGE) {
                item->goal_anim_state = WORKER_3_STATE_PUNCH_2;
            }
            break;

        case WORKER_3_STATE_PUNCH_2:
            if (info.ahead) {
                head = info.angle;
            }
            if (creature->flags != 2
                && (item->touch_bits & WORKER_3_TOUCH_BITS) != 0) {
                Lara_TakeDamage(WORKER_3_SWIPE_DAMAGE, true);
                Creature_Effect(item, &m_Worker3Hit, Spawn_Blood);
                Sound_Effect(SFX_ENEMY_HIT_1, &item->pos, SPM_NORMAL);
                creature->flags = 2;
            }
            break;

        default:
            break;
        }
    }

    Creature_Tilt(item, tilt);
    Creature_Head(item, head);
    Creature_Neck(item, neck);

    if (item->current_anim_state >= WORKER_3_STATE_CLIMB_3) {
        Creature_Animate(item_num, angle, 0);
    } else {
        switch (Creature_Vault(item_num, angle, 2, WORKER_3_VAULT_SHIFT)) {
        case -4:
            Item_SwitchToObjAnim(item, WORKER_3_ANIM_FALL_3, 0, O_WORKER_3);
            item->current_anim_state = WORKER_3_STATE_FALL_3;
            break;

        case 2:
            Item_SwitchToObjAnim(item, WORKER_3_ANIM_CLIMB_1, 0, O_WORKER_3);
            item->current_anim_state = WORKER_3_STATE_CLIMB_1;
            break;

        case 3:
            Item_SwitchToObjAnim(item, WORKER_3_ANIM_CLIMB_2, 0, O_WORKER_3);
            item->current_anim_state = WORKER_3_STATE_CLIMB_2;
            break;

        case 4:
            Item_SwitchToObjAnim(item, WORKER_3_ANIM_CLIMB_3, 0, O_WORKER_3);
            item->current_anim_state = WORKER_3_STATE_CLIMB_3;
            break;

        default:
            return;
        }
    }
}

static void M_SetupBase(OBJECT *const obj)
{
    obj->control_func = M_Control;
    obj->collision_func = Creature_Collision;

    obj->radius = WORKER_RADIUS;
    obj->shadow_size = UNIT_SHADOW / 2;
    obj->pivot_length = 0;
    obj->lot_setup = LOT_Setup(LOT_SETUP_CLIMBER);

    obj->intelligent = true;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_flags = true;
    obj->save_anim = true;

    Object_GetBone(obj, 0)->rot.y = true;
    Object_GetBone(obj, 4)->rot.y = true;
}

static void M_Setup3(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }

    M_SetupBase(obj);
    obj->hit_points = WORKER_3_HITPOINTS;
}

static void M_Setup4(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }

    M_SetupBase(obj);
    obj->hit_points = WORKER_4_HITPOINTS;
}

REGISTER_OBJECT(O_WORKER_3, M_Setup3)
REGISTER_OBJECT(O_WORKER_4, M_Setup4)
