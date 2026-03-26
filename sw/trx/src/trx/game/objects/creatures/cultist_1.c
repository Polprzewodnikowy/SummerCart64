#include <trx/core/utils.h>
#include <trx/debug.h>
#include <trx/game/creature.h>
#include <trx/game/objects.h>
#include <trx/game/objects/creatures/cultist_common.h>
#include <trx/game/random.h>

// clang-format off
#define CULTIST_1_HITPOINTS     25
#define CULTIST_1_SHOOT_DAMAGE  50
#define CULTIST_1_WALK_TURN     (DEG_1 * 5) // = 910
#define CULTIST_1_RUN_TURN      (DEG_1 * 5) // = 910
#define CULTIST_1_RUN_RANGE     SQUARE(WALL_L * 2) // = 4194304
#define CULTIST_1_POSE_CHANCE   0x500 // = 1280
#define CULTIST_1_UNPOSE_CHANCE 0x100 // = 256
#define CULTIST_1_WALK_CHANCE   (CULTIST_1_POSE_CHANCE + 0x500) // = 2560
#define CULTIST_1_UNWALK_CHANCE 0x300 // = 768
// clang-format on

typedef enum {
    // clang-format off
    CULTIST_1_STATE_EMPTY   = 0,
    CULTIST_1_STATE_WALK    = 1,
    CULTIST_1_STATE_RUN     = 2,
    CULTIST_1_STATE_STOP    = 3,
    CULTIST_1_STATE_WAIT_1  = 4,
    CULTIST_1_STATE_WAIT_2  = 5,
    CULTIST_1_STATE_AIM_1   = 6,
    CULTIST_1_STATE_SHOOT_1 = 7,
    CULTIST_1_STATE_AIM_2   = 8,
    CULTIST_1_STATE_SHOOT_2 = 9,
    CULTIST_1_STATE_AIM_3   = 10,
    CULTIST_1_STATE_SHOOT_3 = 11,
    CULTIST_1_STATE_DEATH   = 12,
    // clang-format on
} CULTIST_1_STATE;

typedef enum {
    CULTIST_1_ANIM_DEATH = 20,
} CULTIST_1_ANIM;

static const CREATURE_GUN m_Cultist1Gun = {
    .muzzle = { .pos = { .x = 3, .y = 331, .z = 56 }, .mesh_num = 10 },
};

static void M_Initialise(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    if (Random_GetControl() < 0x4000) {
        item->mesh_bits &= ~0b00110000;
    }
    if (item->object_id == O_CULT_1B) {
        // clang-format off
        // TODO: clang-format >=20 formats this wrongly
        item->mesh_bits &= ~0b00011111'10000000'00000000;
        // clang-format on
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
    int16_t angle = 0;

    if (item->hit_points <= 0) {
        if (item->current_anim_state != CULTIST_1_STATE_DEATH) {
            Item_SwitchToAnim(
                item, Random_GetControl() / 0x4000 + CULTIST_1_ANIM_DEATH, 0);
            item->current_anim_state = CULTIST_1_STATE_DEATH;
        }
    } else {
        AI_INFO info;
        Creature_AIInfo(item, &info);
        Creature_Mood(item, &info, false);

        angle = Creature_Turn(item, creature->maximum_turn);

        switch (item->current_anim_state) {
        case CULTIST_1_STATE_STOP:
            creature->maximum_turn = 0;
            if (item->required_anim_state != CULTIST_1_STATE_EMPTY) {
                item->goal_anim_state = item->required_anim_state;
            }
            break;

        case CULTIST_1_STATE_WAIT_1:
            if (creature->mood == MOOD_ESCAPE) {
                item->required_anim_state = CULTIST_1_STATE_RUN;
                item->goal_anim_state = CULTIST_1_STATE_STOP;
            } else if (Creature_CanTargetEnemy(item, &info)) {
                item->goal_anim_state = CULTIST_1_STATE_STOP;
                item->required_anim_state = Random_GetControl() < 0x4000
                    ? CULTIST_1_STATE_AIM_1
                    : CULTIST_1_STATE_AIM_3;
            } else if (creature->mood == MOOD_BORED && info.ahead) {
                const int16_t random = Random_GetControl();
                if (random < CULTIST_1_POSE_CHANCE) {
                    item->required_anim_state = CULTIST_1_STATE_WAIT_2;
                    item->goal_anim_state = CULTIST_1_STATE_STOP;
                } else if (random < CULTIST_1_WALK_CHANCE) {
                    item->required_anim_state = CULTIST_1_STATE_WALK;
                    item->goal_anim_state = CULTIST_1_STATE_STOP;
                }
            } else if (
                creature->mood == MOOD_BORED
                || info.distance < CULTIST_1_RUN_RANGE) {
                item->required_anim_state = CULTIST_1_STATE_WALK;
                item->goal_anim_state = CULTIST_1_STATE_STOP;
            } else {
                item->required_anim_state = CULTIST_1_STATE_RUN;
                item->goal_anim_state = CULTIST_1_STATE_STOP;
            }
            break;

        case CULTIST_1_STATE_WAIT_2:
            if (Creature_CanTargetEnemy(item, &info)) {
                item->goal_anim_state = CULTIST_1_STATE_STOP;
                item->required_anim_state = CULTIST_1_STATE_AIM_1;
            } else if (
                creature->mood != MOOD_BORED
                || Random_GetControl() < CULTIST_1_UNPOSE_CHANCE
                || !info.ahead) {
                item->goal_anim_state = CULTIST_1_STATE_STOP;
            }
            break;

        case CULTIST_1_STATE_WALK:
            creature->maximum_turn = CULTIST_1_WALK_TURN;
            if (creature->mood == MOOD_ESCAPE) {
                item->goal_anim_state = CULTIST_1_STATE_RUN;
            } else if (Creature_CanTargetEnemy(item, &info)) {
                item->goal_anim_state = CULTIST_1_STATE_STOP;
                item->required_anim_state = Random_GetControl() < 0x4000
                    ? CULTIST_1_STATE_AIM_1
                    : CULTIST_1_STATE_AIM_3;
            } else if (info.distance > CULTIST_1_RUN_RANGE || !info.ahead) {
                item->goal_anim_state = CULTIST_1_STATE_RUN;
            } else if (
                creature->mood == MOOD_BORED && info.ahead
                && Random_GetControl() < CULTIST_1_UNWALK_CHANCE) {
                item->goal_anim_state = CULTIST_1_STATE_STOP;
            }
            break;

        case CULTIST_1_STATE_RUN:
            creature->maximum_turn = CULTIST_1_RUN_TURN;
            creature->flags = 0;
            tilt = angle / 4;
            if (creature->mood == MOOD_ESCAPE) {
                if (Creature_CanTargetEnemy(item, &info)) {
                    item->goal_anim_state = CULTIST_1_STATE_SHOOT_2;
                }
            } else if (Creature_CanTargetEnemy(item, &info)) {
                if (info.distance < CULTIST_1_RUN_RANGE
                    || info.zone_num != info.enemy_zone_num) {
                    item->goal_anim_state = CULTIST_1_STATE_STOP;
                } else {
                    item->goal_anim_state = CULTIST_1_STATE_SHOOT_2;
                }
            } else if (creature->mood == MOOD_BORED) {
                item->goal_anim_state = CULTIST_1_STATE_STOP;
            }
            break;

        case CULTIST_1_STATE_AIM_1:
        case CULTIST_1_STATE_AIM_3:
            creature->flags = 0;
            if (info.ahead) {
                head = info.angle;
            }
            if (creature->mood == MOOD_ESCAPE) {
                item->goal_anim_state = CULTIST_1_STATE_STOP;
            } else if (Creature_CanTargetEnemy(item, &info)) {
                item->goal_anim_state =
                    item->current_anim_state == CULTIST_1_STATE_AIM_1
                    ? CULTIST_1_STATE_SHOOT_1
                    : CULTIST_1_STATE_SHOOT_3;
            } else {
                item->goal_anim_state = CULTIST_1_STATE_STOP;
            }
            break;

        case CULTIST_1_STATE_SHOOT_1:
        case CULTIST_1_STATE_SHOOT_3:
            if (info.ahead) {
                head = info.angle;
            }
            if (creature->flags == 0) {
                Creature_Shoot(
                    item, &info, &m_Cultist1Gun, head, CULTIST_1_SHOOT_DAMAGE);
                creature->flags = 1;
            }
            break;

        case CULTIST_1_STATE_SHOOT_2:
            if (info.ahead) {
                head = info.angle;
            }
            if (item->required_anim_state == CULTIST_1_STATE_EMPTY) {
                if (!Creature_Shoot(
                        item, &info, &m_Cultist1Gun, head,
                        CULTIST_1_SHOOT_DAMAGE)) {
                    item->goal_anim_state = CULTIST_1_STATE_RUN;
                }
                item->required_anim_state = CULTIST_1_STATE_SHOOT_2;
            }
            break;

        default:
            break;
        }
    }

    Creature_Tilt(item, tilt);
    Creature_Head(item, head);
    Creature_Animate(item_num, angle, 0);
}

static void M_Setup1(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }

    obj->initialise_func = M_Initialise;
    obj->control_func = M_Control;
    obj->collision_func = Creature_Collision;

    obj->hit_points = CULTIST_1_HITPOINTS;
    obj->radius = CULTIST_RADIUS;
    obj->shadow_size = UNIT_SHADOW / 2;
    obj->pivot_length = 50;

    obj->intelligent = true;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_flags = true;
    obj->save_anim = true;

    Object_GetBone(obj, 0)->rot.y = true;
}

static void M_Setup1A(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }

    const OBJECT *const ref_obj = Object_Get(O_CULT_1);
    if (ref_obj->loaded) {
        obj->frame_base = ref_obj->frame_base;
        obj->anim_idx = ref_obj->anim_idx;
    }

    obj->initialise_func = M_Initialise;
    obj->control_func = M_Control;
    obj->collision_func = Creature_Collision;

    obj->hit_points = CULTIST_1_HITPOINTS;
    obj->radius = CULTIST_RADIUS;
    obj->shadow_size = UNIT_SHADOW / 2;
    obj->pivot_length = 50;

    obj->intelligent = true;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_flags = true;
    obj->save_anim = true;

    Object_GetBone(obj, 0)->rot.y = true;
}

static void M_Setup1B(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }

    const OBJECT *const ref_obj = Object_Get(O_CULT_1);
    if (ref_obj->loaded) {
        obj->frame_base = ref_obj->frame_base;
        obj->anim_idx = ref_obj->anim_idx;
    }

    obj->initialise_func = M_Initialise;
    obj->control_func = M_Control;
    obj->collision_func = Creature_Collision;

    obj->hit_points = CULTIST_1_HITPOINTS;
    obj->radius = CULTIST_RADIUS;
    obj->shadow_size = UNIT_SHADOW / 2;
    obj->pivot_length = 50;

    obj->intelligent = true;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_flags = true;
    obj->save_anim = true;

    Object_GetBone(obj, 0)->rot.y = true;
}

REGISTER_OBJECT(O_CULT_1, M_Setup1)
REGISTER_OBJECT(O_CULT_1A, M_Setup1A)
REGISTER_OBJECT(O_CULT_1B, M_Setup1B)
