#include <trx/core/utils.h>
#include <trx/game/creature.h>
#include <trx/game/lara.h>
#include <trx/game/pathing.h>
#include <trx/game/rooms.h>
#include <trx/game/spawn.h>

#define BAT_ATTACK_DAMAGE 2
#define BAT_TURN (20 * DEG_1) // = 3640
#define BAT_HITPOINTS 1
#define BAT_RADIUS (WALL_L / 10) // = 102
#define BAT_SMARTNESS 0x400

typedef enum {
    BAT_STATE_EMPTY = 0,
    BAT_STATE_STOP = 1,
    BAT_STATE_FLY = 2,
    BAT_STATE_ATTACK = 3,
    BAT_STATE_FALL = 4,
    BAT_STATE_DEATH = 5,
} BAT_STATE;

static BITE m_BatBite = { .pos = { 0, 16, 45 }, .mesh_num = 4 };

static void M_FixEmbeddedPosition(int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    if (item->status == IS_ACTIVE) {
        return;
    }

    int16_t room_num = item->room_num;
    const SECTOR *const sector = Room_GetSector(item->pos, &room_num);
    const int32_t ceiling = Room_GetCeiling(sector, item->pos);

    // The bats animation and frame have to be changed to the hanging
    // one to properly measure them. Save it so it can be restored
    // after.
    const int16_t old_anim = Item_GetRelativeAnim(item);
    const int16_t old_frame = Item_GetRelativeFrame(item);

    Item_SwitchToAnim(item, 0, 0);
    const BOUNDS_16 *const bounds = Item_GetBoundsAccurate(item);
    Item_SwitchToAnim(item, old_anim, old_frame);

    const int16_t bat_height = ABS(bounds->min.y);

    // Only move the bat if it's above the calculated position,
    // Palace Midas has many bats that aren't intended to be at
    // ceiling level.
    if (item->pos.y < ceiling + bat_height) {
        item->pos.y = ceiling + bat_height;
    }
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);

    if (item->status == IS_INVISIBLE) {
        if (!LOT_EnableBaddieAI(item_num, 0)) {
            return;
        }
        item->status = IS_ACTIVE;
    }

    CREATURE *const bat = item->creature_data;
    int16_t angle = 0;
    if (item->hit_points <= 0) {
        if (item->pos.y < item->floor) {
            item->gravity = true;
            item->goal_anim_state = BAT_STATE_FALL;
            item->speed = 0;
        } else {
            item->gravity = false;
            item->fall_speed = 0;
            item->goal_anim_state = BAT_STATE_DEATH;
            item->pos.y = item->floor;
        }
        Creature_Animate(item_num, 0, 0);
        return;
    } else {
        AI_INFO info;
        Creature_AIInfo(item, &info);
        Creature_Mood(item, &info, false);
        angle = Creature_Turn(item, BAT_TURN);

        switch (item->current_anim_state) {
        case BAT_STATE_STOP:
            item->goal_anim_state = BAT_STATE_FLY;
            break;

        case BAT_STATE_FLY:
            if (item->touch_bits) {
                item->goal_anim_state = BAT_STATE_ATTACK;
                Creature_Animate(item_num, angle, 0);
                return;
            }
            break;

        case BAT_STATE_ATTACK:
            if (item->touch_bits) {
                Creature_Effect(item, &m_BatBite, Spawn_Blood);
                Lara_TakeDamage(BAT_ATTACK_DAMAGE, true);
            } else {
                item->goal_anim_state = BAT_STATE_FLY;
                bat->mood = MOOD_BORED;
            }
            break;
        }
    }

    Creature_Animate(item_num, angle, 0);
}

static void M_Initialise(const int16_t item_num)
{
    Creature_Initialise(item_num);

    // Almost all of the bats in the OG levels are embedded in the ceiling.
    // This will move all bats up to the ceiling of their rooms and down
    // by the height of their hanging animation.
    M_FixEmbeddedPosition(item_num);
}

static void M_Setup(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }
    obj->initialise_func = M_Initialise;
    obj->control_func = M_Control;
    obj->collision_func = Creature_Collision;
    obj->shadow_size = UNIT_SHADOW / 2;
    obj->hit_points = BAT_HITPOINTS;
    obj->radius = BAT_RADIUS;
    obj->smartness = BAT_SMARTNESS;
    obj->lot_setup = LOT_Setup(LOT_SETUP_FLYER);
    obj->intelligent = true;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_anim = true;
    obj->save_flags = true;
}

REGISTER_OBJECT(O_BAT, M_Setup)
