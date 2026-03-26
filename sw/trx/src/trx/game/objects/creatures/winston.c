#include <trx/core/utils.h>
#include <trx/game/creature.h>
#include <trx/game/objects.h>
#include <trx/game/pathing.h>
#include <trx/game/random.h>
#include <trx/game/sound.h>
#include <trx/game/spawn.h>

// clang-format off
#define M_RADIUS     (WALL_L / 10) // = 102
#define M_STOP_RANGE SQUARE(WALL_L * 3 / 2) // = 2359296
// clang-format on

typedef enum {
    // clang-format off
    WINSTON_STATE_EMPTY = 0,
    WINSTON_STATE_STOP  = 1,
    WINSTON_STATE_WALK  = 2,
    // clang-format on
} M_STATE;

static bool M_IsAlive(const ITEM *const item)
{
    return item->hit_points > 0;
}

static bool M_IsTargetable(const ITEM *const item)
{
    return false;
}

static bool M_CanTakeDamage(const ITEM *const item)
{
    return false;
}

static bool M_CanBeProjectileTarget(const ITEM *const item)
{
    return false;
}

static bool M_ShouldSpawnBlood(const ITEM *const item)
{
    return false;
}

static void M_Control(const int16_t item_num)
{
    if (!Creature_Activate(item_num)) {
        return;
    }

    ITEM *const item = Item_Get(item_num);
    CREATURE *const creature = item->creature_data;
    if (creature == nullptr) {
        return;
    }

    AI_INFO info;
    Creature_AIInfo(item, &info);
    Creature_Mood(item, &info, true);

    const int16_t angle = Creature_Turn(item, creature->maximum_turn);

    if (item->current_anim_state == WINSTON_STATE_STOP) {
        if (item->goal_anim_state != WINSTON_STATE_WALK
            && (info.distance > M_STOP_RANGE || !info.ahead)) {
            item->goal_anim_state = WINSTON_STATE_WALK;
            Sound_Effect(SFX_WINSTON_GRUNT_2, &item->pos, SPM_NORMAL);
        }
    } else if (info.distance < M_STOP_RANGE) {
        if (info.ahead) {
            item->goal_anim_state = WINSTON_STATE_STOP;
            if ((creature->flags & 1) != 0) {
                creature->flags--;
            }
        } else if ((creature->flags & 1) == 0) {
            Sound_Effect(SFX_WINSTON_GRUNT_1, &item->pos, SPM_NORMAL);
            Sound_Effect(SFX_WINSTON_CUPS, &item->pos, SPM_NORMAL);
            creature->flags |= 1;
        }
    }

    if (item->touch_bits != 0 && (creature->flags & 2) == 0) {
        Sound_Effect(SFX_WINSTON_GRUNT_3, &item->pos, SPM_NORMAL);
        Sound_Effect(SFX_WINSTON_CUPS, &item->pos, SPM_NORMAL);
        creature->flags |= 2;
    } else if (item->touch_bits == 0 && (creature->flags & 2) != 0) {
        creature->flags -= 2;
    }

    if (Random_GetDraw() < 0x100) {
        Sound_Effect(SFX_WINSTON_CUPS, &item->pos, SPM_NORMAL);
    }

    Creature_Animate(item_num, angle, 0);
}

static void M_Setup(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }

    obj->control_func = M_Control;
    obj->collision_func = Object_Collision;
    obj->should_spawn_blood_func = M_ShouldSpawnBlood;
    obj->is_alive_func = M_IsAlive;
    obj->is_targetable_func = M_IsTargetable;
    obj->can_take_damage_func = M_CanTakeDamage;
    obj->can_be_projectile_target_func = M_CanBeProjectileTarget;

    obj->hit_points = 1;
    obj->radius = M_RADIUS;
    obj->shadow_size = UNIT_SHADOW / 4;
    obj->smartness = -1;

    obj->intelligent = true;
    obj->save_position = true;
    obj->save_flags = true;
    obj->save_anim = true;
}

REGISTER_OBJECT(O_WINSTON, M_Setup)
