#include <trx/core/utils.h>
#include <trx/game/creature.h>
#include <trx/game/lara.h>
#include <trx/game/pathing.h>
#include <trx/game/spawn.h>

// clang-format off
#define M_DAMAGE            20
#define M_RADIUS            (WALL_L / 5) // = 204
#define M_ATTACK_RANGE      SQUARE(WALL_L / 2) // = 262144
#define M_TURN              (DEG_1 * 3) // = 546
#define M_START_ANIM        5
#define M_DIE_ANIM          8
#define M_EAGLE_HITPOINTS   20
#define M_CROW_HITPOINTS    (g_TRVersion == 3 ? 8 : 15)
#define M_CROW_START_ANIM   14
#define M_CROW_DIE_ANIM     1
#define M_VULTURE_HITPOINTS 18
// clang-format on

typedef enum {
    M_STATE_EMPTY = 0,
    M_STATE_FLY = 1,
    M_STATE_STOP = 2,
    M_STATE_GLIDE = 3,
    M_STATE_FALL = 4,
    M_STATE_DEATH = 5,
    M_STATE_ATTACK = 6,
    M_STATE_EAT = 7,
} M_STATE;

static const BITE m_BirdBite = {
    .pos = { .x = 15, .y = 46, .z = 21 },
    .mesh_num = 6,
};
static const BITE m_CrowBite = {
    .pos = { .x = 2, .y = 10, .z = 60 },
    .mesh_num = 14,
};

static void M_Initialise(const int16_t item_num)
{
    Creature_Initialise(item_num);
    ITEM *const item = Item_Get(item_num);
    if (item->object_id == O_CROW) {
        Item_SwitchToAnim(item, M_CROW_START_ANIM, 0);
        item->goal_anim_state = M_STATE_EAT;
        item->current_anim_state = M_STATE_EAT;
    } else {
        Item_SwitchToAnim(item, M_START_ANIM, 0);
        item->goal_anim_state = M_STATE_STOP;
        item->current_anim_state = M_STATE_STOP;
    }
}

static void M_Control(const int16_t item_num)
{
    if (!Creature_Activate(item_num)) {
        return;
    }

    ITEM *const item = Item_Get(item_num);
    CREATURE *const bird = item->creature_data;

    if (item->hit_points <= 0) {
        switch (item->current_anim_state) {
        case M_STATE_FALL:
            if (item->pos.y > item->floor) {
                item->pos.y = item->floor;
                item->gravity = false;
                item->fall_speed = 0;
                item->goal_anim_state = M_STATE_DEATH;
            }
            break;

        case M_STATE_DEATH:
            item->pos.y = item->floor;
            break;

        default:
            const int16_t anim_idx =
                item->object_id == O_CROW ? M_CROW_DIE_ANIM : M_DIE_ANIM;
            Item_SwitchToAnim(item, anim_idx, 0);
            item->current_anim_state = M_STATE_FALL;
            item->gravity = true;
            item->speed = 0;
            break;
        }
        item->rot.x = 0;
        Creature_Animate(item_num, 0, 0);
        return;
    }

    AI_INFO info;
    Creature_AIInfo(item, &info);
    Creature_Mood(item, &info, false);

    const int16_t angle = Creature_Turn(item, M_TURN);

    switch (item->current_anim_state) {
    case M_STATE_FLY:
        bird->flags = 0;
        if (item->required_anim_state != M_STATE_EMPTY) {
            item->goal_anim_state = item->required_anim_state;
        }
        if (bird->mood == MOOD_BORED) {
            item->goal_anim_state = M_STATE_STOP;
        } else if (info.ahead && info.distance < M_ATTACK_RANGE) {
            item->goal_anim_state = M_STATE_ATTACK;
        } else {
            item->goal_anim_state = M_STATE_GLIDE;
        }
        break;

    case M_STATE_STOP:
    case M_STATE_EAT:
        item->pos.y = item->floor;
        if (bird->mood != MOOD_BORED) {
            item->goal_anim_state = M_STATE_FLY;
        }
        break;

    case M_STATE_GLIDE:
        if (bird->mood == MOOD_BORED) {
            item->required_anim_state = M_STATE_STOP;
            item->goal_anim_state = M_STATE_FLY;
        } else if (info.ahead && info.distance < M_ATTACK_RANGE) {
            item->goal_anim_state = M_STATE_ATTACK;
        }
        break;

    case M_STATE_ATTACK:
        if (bird->flags == 0 && item->touch_bits != 0) {
            Lara_TakeDamage(M_DAMAGE, true);
            if (item->object_id == O_CROW) {
                Creature_Effect(item, &m_CrowBite, Spawn_Blood);
            } else {
                Creature_Effect(item, &m_BirdBite, Spawn_Blood);
            }
            bird->flags = 1;
        }
        break;
    }

    Creature_Animate(item_num, angle, 0);
}

static bool M_SetupCommon(OBJECT *const obj)
{
    if (!obj->loaded) {
        return false;
    }

    obj->initialise_func = M_Initialise;
    obj->control_func = M_Control;
    obj->collision_func = Creature_Collision;

    obj->radius = M_RADIUS;
    obj->shadow_size = UNIT_SHADOW / 2;
    obj->pivot_length = 0;
    obj->lot_setup = LOT_Setup(LOT_SETUP_FLYER);

    obj->intelligent = true;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_flags = true;
    obj->save_anim = true;

    return true;
}

static void M_SetupEagle(OBJECT *const obj)
{
    if (!M_SetupCommon(obj)) {
        return;
    }

    obj->hit_points = M_EAGLE_HITPOINTS;
}

static void M_SetupCrow(OBJECT *const obj)
{
    if (!M_SetupCommon(obj)) {
        return;
    }

    obj->hit_points = M_CROW_HITPOINTS;
}

static void M_SetupVulture(OBJECT *const obj)
{
    if (!M_SetupCommon(obj)) {
        return;
    }

    obj->hit_points = M_VULTURE_HITPOINTS;
}

REGISTER_OBJECT(O_EAGLE, M_SetupEagle)
REGISTER_OBJECT(O_CROW, M_SetupCrow)
REGISTER_OBJECT(O_VULTURE, M_SetupVulture)
