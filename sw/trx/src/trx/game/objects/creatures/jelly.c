#include <trx/game/creature.h>
#include <trx/game/lara.h>
#include <trx/game/objects/common.h>
#include <trx/game/pathing.h>
#include <trx/game/spawn.h>

// clang-format off
#define JELLY_HITPOINTS    10
#define JELLY_RADIUS       (WALL_L / 10) // = 102
#define JELLY_STING_DAMAGE 5
#define JELLY_TURN         (DEG_1 * 90) // = 16380
// clang-format on

typedef enum {
    // clang-format off
    JELLY_STATE_EMPTY = 0,
    JELLY_STATE_MOVE  = 1,
    JELLY_STATE_STOP  = 2,
    // clang-format on
} JELLY_STATE;

static void M_Control(const int16_t item_num)
{
    if (!Creature_Activate(item_num)) {
        return;
    }

    ITEM *const item = Item_Get(item_num);
    CREATURE *const creature = item->creature_data;

    if (item->hit_points <= 0) {
        if (Item_Explode(item_num, -1, 0)) {
            LOT_DisableBaddieAI(item_num);
            Item_Kill(item_num);
            item->status = IS_DEACTIVATED;
        }
    } else {
        AI_INFO info;
        Creature_AIInfo(item, &info);
        Creature_Mood(item, &info, false);

        int16_t angle = Creature_Turn(item, JELLY_TURN);

        switch (item->current_anim_state) {
        case JELLY_STATE_STOP:
            if (creature->mood != MOOD_BORED) {
                item->goal_anim_state = JELLY_STATE_MOVE;
            }
            break;

        case JELLY_STATE_MOVE:
            if (creature->mood == MOOD_BORED || item->touch_bits != 0) {
                item->goal_anim_state = JELLY_STATE_STOP;
            }
            break;
        }

        if (item->touch_bits != 0) {
            Lara_TakeDamage(JELLY_STING_DAMAGE, true);
        }

        Creature_Head(item, 0);
        Creature_Animate(item_num, angle, 0);
        Creature_Underwater(item, 0);
    }
}

static void M_Setup(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }

    obj->control_func = M_Control;
    obj->collision_func = Creature_Collision;

    obj->hit_points = JELLY_HITPOINTS;
    obj->radius = JELLY_RADIUS;
    obj->shadow_size = UNIT_SHADOW / 2;
    obj->lot_setup = LOT_Setup(LOT_SETUP_FLYER);

    obj->intelligent = true;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_flags = true;
    obj->save_anim = true;
}

REGISTER_OBJECT(O_JELLY, M_Setup)
