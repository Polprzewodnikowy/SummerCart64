#include <trx/game/objects/creatures/big_spider.h>

#include <trx/game/creature.h>
#include <trx/game/lara.h>
#include <trx/game/objects/common.h>
#include <trx/game/spawn.h>

// clang-format off
#define BIG_SPIDER_HITPOINTS 40
#define BIG_SPIDER_RADIUS    (WALL_L / 3) // = 341
#define BIG_SPIDER_TURN      (DEG_1 * 4) // = 728
#define BIG_SPIDER_DAMAGE    100
// clang-format on

typedef enum {
    // clang-format off
    BIG_SPIDER_STATE_EMPTY    = 0,
    BIG_SPIDER_STATE_STOP     = 1,
    BIG_SPIDER_STATE_WALK_1   = 2,
    BIG_SPIDER_STATE_WALK_2   = 3,
    BIG_SPIDER_STATE_ATTACK_1 = 4,
    BIG_SPIDER_STATE_ATTACK_2 = 5,
    BIG_SPIDER_STATE_ATTACK_3 = 6,
    BIG_SPIDER_STATE_DEATH    = 7,
    // clang-format on
} BIG_SPIDER_STATE;

typedef enum {
    BIG_SPIDER_ANIM_DEATH = 2,
} BIG_SPIDER_ANIM;

static const BITE m_SpiderBite = {
    .pos = { .x = 0, .y = 0, .z = 41 },
    .mesh_num = 1,
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

    if (item->hit_points > 0) {
        AI_INFO info;
        Creature_AIInfo(item, &info);
        Creature_Mood(item, &info, true);

        angle = Creature_Turn(item, BIG_SPIDER_TURN);

        switch (item->current_anim_state) {
        case BIG_SPIDER_STATE_STOP:
            creature->flags = 0;
            if (creature->mood == MOOD_BORED) {
                break;
            } else if (info.ahead && item->touch_bits != 0) {
                item->goal_anim_state = BIG_SPIDER_STATE_ATTACK_1;
            } else if (creature->mood == MOOD_STALK) {
                item->goal_anim_state = BIG_SPIDER_STATE_WALK_1;
            } else if (
                creature->mood == MOOD_ESCAPE
                || creature->mood == MOOD_ATTACK) {
                item->goal_anim_state = BIG_SPIDER_STATE_WALK_2;
            }
            break;

        case BIG_SPIDER_STATE_WALK_1:
            if (creature->mood == MOOD_BORED) {
                break;
            } else if (info.ahead && item->touch_bits != 0) {
                item->goal_anim_state = BIG_SPIDER_STATE_STOP;
            } else if (
                creature->mood == MOOD_ESCAPE
                || creature->mood == MOOD_ATTACK) {
                item->goal_anim_state = BIG_SPIDER_STATE_WALK_2;
            }
            break;

        case BIG_SPIDER_STATE_WALK_2:
            creature->flags = 0;
            if (info.ahead && item->touch_bits != 0) {
                item->goal_anim_state = BIG_SPIDER_STATE_STOP;
            } else if (
                creature->mood == MOOD_BORED || creature->mood == MOOD_STALK) {
                item->goal_anim_state = BIG_SPIDER_STATE_WALK_1;
            }
            break;

        case BIG_SPIDER_STATE_ATTACK_1:
            if (!creature->flags && item->touch_bits != 0) {
                Lara_TakeDamage(BIG_SPIDER_DAMAGE, true);
                Creature_Effect(item, &m_SpiderBite, Spawn_Blood);
                creature->flags = 1;
            }
            break;

        default:
            break;
        }
    } else if (item->current_anim_state != BIG_SPIDER_STATE_DEATH) {
        Item_SwitchToAnim(item, BIG_SPIDER_ANIM_DEATH, 0);
        item->current_anim_state = BIG_SPIDER_STATE_DEATH;
    }

    Creature_Animate(item_num, angle, tilt);
}

void BigSpider_Setup(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }

    obj->control_func = M_Control;
    obj->collision_func = Creature_Collision;

    obj->hit_points = BIG_SPIDER_HITPOINTS;
    obj->radius = BIG_SPIDER_RADIUS;
    obj->shadow_size = UNIT_SHADOW / 2;

    obj->intelligent = true;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_flags = true;
    obj->save_anim = true;
}

REGISTER_OBJECT(O_BIG_SPIDER, BigSpider_Setup)
