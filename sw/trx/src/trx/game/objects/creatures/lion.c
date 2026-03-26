#include <trx/core/utils.h>
#include <trx/game/creature.h>
#include <trx/game/lara.h>
#include <trx/game/pathing.h>
#include <trx/game/random.h>
#include <trx/game/spawn.h>

#define LION_BITE_DAMAGE 250
#define LION_POUNCE_DAMAGE 150
#define LION_TOUCH 0x380066
#define LION_WALK_TURN (2 * DEG_1) // = 364
#define LION_RUN_TURN (5 * DEG_1) // = 910
#define LION_ROAR_CHANCE 128
#define LION_POUNCE_RANGE SQUARE(WALL_L) // = 1048576
#define LION_DIE_ANIM 7
#define LION_HITPOINTS 30
#define LION_RADIUS (WALL_L / 3) // = 341
#define LION_SMARTNESS 0x7FFF

#define LIONESS_HITPOINTS 25
#define LIONESS_RADIUS (WALL_L / 3) // = 341
#define LIONESS_SMARTNESS 0x2000

#define PUMA_DIE_ANIM 4
#define PUMA_HITPOINTS 45
#define PUMA_RADIUS (WALL_L / 3) // = 341
#define PUMA_SMARTNESS 0x2000

typedef enum {
    LION_STATE_EMPTY = 0,
    LION_STATE_STOP = 1,
    LION_STATE_WALK = 2,
    LION_STATE_RUN = 3,
    LION_STATE_ATTACK_1 = 4,
    LION_STATE_DEATH = 5,
    LION_STATE_WARNING = 6,
    LION_STATE_ATTACK_2 = 7,
} LION_STATE;

static BITE m_LionBite = { .pos = { -2, -10, 132 }, .mesh_num = 21 };

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);

    if (item->status == IS_INVISIBLE) {
        if (!LOT_EnableBaddieAI(item_num, 0)) {
            return;
        }
        item->status = IS_ACTIVE;
    }

    CREATURE *const lion = item->creature_data;
    int16_t head = 0;
    int16_t angle = 0;
    int16_t tilt = 0;

    if (item->hit_points <= 0) {
        if (item->current_anim_state != LION_STATE_DEATH) {
            item->current_anim_state = LION_STATE_DEATH;
            int16_t anim_idx =
                item->object_id == O_PUMA ? PUMA_DIE_ANIM : LION_DIE_ANIM;
            Item_SwitchToAnim(
                item, anim_idx + (int16_t)(Random_GetControl() / 0x4000), 0);
        }
    } else {
        AI_INFO info;
        Creature_AIInfo(item, &info);

        if (info.ahead) {
            head = info.angle;
        }

        Creature_Mood(item, &info, true);

        angle = Creature_Turn(item, lion->maximum_turn);

        switch (item->current_anim_state) {
        case LION_STATE_STOP:
            if (item->required_anim_state) {
                item->goal_anim_state = item->required_anim_state;
            } else if (lion->mood == MOOD_BORED) {
                item->goal_anim_state = LION_STATE_WALK;
            } else if (info.ahead && (item->touch_bits & LION_TOUCH)) {
                item->goal_anim_state = LION_STATE_ATTACK_2;
            } else if (info.ahead && info.distance < LION_POUNCE_RANGE) {
                item->goal_anim_state = LION_STATE_ATTACK_1;
            } else {
                item->goal_anim_state = LION_STATE_RUN;
            }
            break;

        case LION_STATE_WALK:
            lion->maximum_turn = LION_WALK_TURN;
            if (lion->mood != MOOD_BORED) {
                item->goal_anim_state = LION_STATE_STOP;
            } else if (Random_GetControl() < LION_ROAR_CHANCE) {
                item->required_anim_state = LION_STATE_WARNING;
                item->goal_anim_state = LION_STATE_STOP;
            }
            break;

        case LION_STATE_RUN:
            lion->maximum_turn = LION_RUN_TURN;
            tilt = angle;
            if (lion->mood == MOOD_BORED) {
                item->goal_anim_state = LION_STATE_STOP;
            } else if (info.ahead && info.distance < LION_POUNCE_RANGE) {
                item->goal_anim_state = LION_STATE_STOP;
            } else if ((item->touch_bits & LION_TOUCH) && info.ahead) {
                item->goal_anim_state = LION_STATE_STOP;
            } else if (
                lion->mood != MOOD_ESCAPE
                && Random_GetControl() < LION_ROAR_CHANCE) {
                item->required_anim_state = LION_STATE_WARNING;
                item->goal_anim_state = LION_STATE_STOP;
            }
            break;

        case LION_STATE_ATTACK_1:
            if (item->required_anim_state == LION_STATE_EMPTY
                && (item->touch_bits & LION_TOUCH)) {
                Lara_TakeDamage(LION_POUNCE_DAMAGE, true);
                item->required_anim_state = LION_STATE_STOP;
            }
            break;

        case LION_STATE_ATTACK_2:
            if (item->required_anim_state == LION_STATE_EMPTY
                && (item->touch_bits & LION_TOUCH)) {
                Creature_Effect(item, &m_LionBite, Spawn_Blood);
                Lara_TakeDamage(LION_BITE_DAMAGE, true);
                item->required_anim_state = LION_STATE_STOP;
            }
            break;
        }
    }

    Creature_Tilt(item, tilt);
    Creature_Head(item, head);
    Creature_Animate(item_num, angle, tilt);
}

static void M_SetupBase(OBJECT *const obj)
{
    obj->initialise_func = Creature_Initialise;
    obj->control_func = M_Control;
    obj->collision_func = Creature_Collision;
    obj->shadow_size = UNIT_SHADOW / 2;
    obj->lot_setup = LOT_Setup(LOT_SETUP_QUADRUPED);
    obj->pivot_length = 400;
    obj->intelligent = true;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_anim = true;
    obj->save_flags = true;

    Object_GetBone(obj, 19)->rot.y = true;
}

static void M_SetupLion(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }
    M_SetupBase(obj);
    obj->hit_points = LION_HITPOINTS;
    obj->radius = LION_RADIUS;
    obj->smartness = LION_SMARTNESS;
}

static void M_SetupLioness(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }
    M_SetupBase(obj);
    obj->hit_points = LIONESS_HITPOINTS;
    obj->radius = LIONESS_RADIUS;
    obj->smartness = LIONESS_SMARTNESS;
}

static void M_SetupPuma(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }
    M_SetupBase(obj);
    obj->hit_points = PUMA_HITPOINTS;
    obj->radius = PUMA_RADIUS;
    obj->smartness = PUMA_SMARTNESS;
}

REGISTER_OBJECT(O_LION, M_SetupLion)
REGISTER_OBJECT(O_LIONESS, M_SetupLioness)
REGISTER_OBJECT(O_PUMA, M_SetupPuma)
