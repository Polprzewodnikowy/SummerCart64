#include <trx/core/utils.h>
#include <trx/game/creature.h>
#include <trx/game/items/carrier.h>
#include <trx/game/lara.h>
#include <trx/game/objects/common.h>
#include <trx/game/pathing.h>
#include <trx/game/random.h>
#include <trx/game/sound.h>
#include <trx/game/spawn.h>

// clang-format off
#define SPIDER_HITPOINTS      5
#define SPIDER_TURN           (DEG_1 * 8) // = 1456
#define SPIDER_RADIUS         (WALL_L / 10) // = 102
#define SPIDER_ATTACK_2_RANGE SQUARE(WALL_L / 2) // = 262144
#define SPIDER_ATTACK_3_RANGE SQUARE(WALL_L / 5) // = 41616
#define SPIDER_DAMAGE         25
// clang-format on

typedef enum {
    // clang-format off
    SPIDER_STATE_EMPTY    = 0,
    SPIDER_STATE_STOP     = 1,
    SPIDER_STATE_WALK_1   = 2,
    SPIDER_STATE_WALK_2   = 3,
    SPIDER_STATE_ATTACK_1 = 4,
    SPIDER_STATE_ATTACK_2 = 5,
    SPIDER_STATE_ATTACK_3 = 6,
    SPIDER_STATE_DEATH    = 7,
    // clang-format on
} SPIDER_STATE;

typedef enum {
    SPIDER_ANIM_LEAP = 2,
} SPIDER_ANIM;

static const BITE m_SpiderBite = {
    .pos = { .x = 0, .y = 0, .z = 41 },
    .mesh_num = 1,
};

static void M_Leap(const int16_t item_num, const int16_t angle)
{
    ITEM *const item = Item_Get(item_num);
    const XYZ_32 old_pos = item->pos;
    const int16_t old_room_num = item->room_num;

    Creature_Animate(item_num, angle, 0);
    if (item->pos.y > old_pos.y - STEP_L * 3 / 2) {
        return;
    }

    item->pos = old_pos;
    Item_UpdateRoom(item_num, old_room_num);
    Item_SwitchToAnim(item, SPIDER_ANIM_LEAP, 0);
    item->current_anim_state = SPIDER_STATE_ATTACK_2;
    Creature_Animate(item_num, angle, 0);
}

static void M_Control(const int16_t item_num)
{
    if (!Creature_Activate(item_num)) {
        return;
    }

    int16_t angle = 0;
    ITEM *const item = Item_Get(item_num);
    CREATURE *const creature = item->creature_data;

    if (item->hit_points > 0) {
        AI_INFO info;
        Creature_AIInfo(item, &info);
        Creature_Mood(item, &info, false);

        angle = Creature_Turn(item, SPIDER_TURN);

        switch (item->current_anim_state) {
        case SPIDER_STATE_STOP:
            creature->flags = 0;
            if (creature->mood == MOOD_BORED) {
                if (Random_GetControl() < 256) {
                    item->goal_anim_state = SPIDER_STATE_WALK_1;
                }
            } else if (info.ahead && item->touch_bits != 0) {
                item->goal_anim_state = SPIDER_STATE_ATTACK_1;
            } else if (creature->mood == MOOD_STALK) {
                item->goal_anim_state = SPIDER_STATE_WALK_1;
            } else if (
                creature->mood == MOOD_ESCAPE
                || creature->mood == MOOD_ATTACK) {
                item->goal_anim_state = SPIDER_STATE_WALK_2;
            }
            break;

        case SPIDER_STATE_WALK_1:
            if (creature->mood == MOOD_BORED) {
                if (Random_GetControl() < 256) {
                    item->goal_anim_state = SPIDER_STATE_STOP;
                }
            } else if (
                creature->mood == MOOD_ESCAPE
                || creature->mood == MOOD_ATTACK) {
                item->goal_anim_state = SPIDER_STATE_WALK_2;
            }
            break;

        case SPIDER_STATE_WALK_2:
            creature->flags = 0;
            if (creature->mood == MOOD_BORED || creature->mood == MOOD_STALK) {
                item->goal_anim_state = SPIDER_STATE_WALK_1;
            } else if (info.ahead && item->touch_bits != 0) {
                item->goal_anim_state = SPIDER_STATE_STOP;
            } else if (info.ahead && info.distance < SPIDER_ATTACK_3_RANGE) {
                item->goal_anim_state = SPIDER_STATE_ATTACK_3;
            } else if (info.ahead && info.distance < SPIDER_ATTACK_2_RANGE) {
                item->goal_anim_state = SPIDER_STATE_ATTACK_2;
            }
            break;

        case SPIDER_STATE_ATTACK_1:
        case SPIDER_STATE_ATTACK_2:
        case SPIDER_STATE_ATTACK_3:
            if (creature->flags == 0 && item->touch_bits != 0) {
                Creature_Effect(item, &m_SpiderBite, Spawn_Blood);
                Lara_TakeDamage(SPIDER_DAMAGE, true);
                creature->flags = 1;
            }
            break;

        default:
            break;
        }
    } else if (Item_Explode(item_num, -1, 0)) {
        LOT_DisableBaddieAI(item_num);
        Item_Kill(item_num);
        item->status = IS_DEACTIVATED;
        Sound_Effect(SFX_SPIDER_EXPLODE, &item->pos, SPM_NORMAL);
        Carrier_TestItemDrops(item_num);
        return;
    }

    M_Leap(item_num, angle);
}

static void M_Setup(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }

    obj->control_func = M_Control;
    obj->collision_func = Creature_Collision;

    obj->hit_points = SPIDER_HITPOINTS;
    obj->radius = SPIDER_RADIUS;
    obj->shadow_size = UNIT_SHADOW / 2;
    obj->lot_setup = LOT_Setup(LOT_SETUP_JUMPER);

    obj->intelligent = true;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_flags = true;
    obj->save_anim = true;
}

REGISTER_OBJECT(O_SPIDER, M_Setup)
