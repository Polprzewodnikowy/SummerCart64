#include <trx/game/collision/los.h>
#include <trx/game/creature.h>
#include <trx/game/effects.h>
#include <trx/game/items/carrier.h>
#include <trx/game/lara.h>
#include <trx/game/pathing.h>
#include <trx/game/rooms.h>
#include <trx/game/spawn.h>

// clang-format off
#define M_SWIM_TURN (3 * DEG_1)  // = 546
#define M_FRONT_ARC DEG_45
#define M_HITPOINTS 20
#define M_RADIUS    (WALL_L / 3) // = 341
// clang-format on

typedef enum {
    M_STATE_EMPTY,
    M_STATE_SWIM_1,
    M_STATE_SWIM_2,
    M_STATE_SHOOT_1,
    M_STATE_AIM_1,
    M_STATE_NULL_1,
    M_STATE_AIM_2,
    M_STATE_SHOOT_2,
    M_STATE_NULL_2,
    M_STATE_DEATH,
    // clang-format on
} DIVER_STATE;

typedef enum {
    M_ANIM_DEATH = 16,
} M_ANIM;

static const BITE m_DiverBite = {
    .pos = { .x = 17, .y = 164, .z = 44 },
    .mesh_num = 18,
};

static int32_t M_GetWaterSurface(
    const int32_t x, const int32_t y, const int32_t z, const int16_t room_num)
{
    const ROOM *room = Room_Get(room_num);
    const SECTOR *sector = Room_GetWorldSector(room, x, z);

    if (room->flags.underwater) {
        while (sector->portal_room.sky != NO_ROOM) {
            room = Room_Get(sector->portal_room.sky);
            if (!room->flags.underwater) {
                return sector->ceiling.height;
            }
            sector = Room_GetWorldSector(room, x, z);
        }
    } else {
        while (sector->portal_room.pit != NO_ROOM) {
            room = Room_Get(sector->portal_room.pit);
            if (room->flags.underwater) {
                return sector->floor.height;
            }
            sector = Room_GetWorldSector(room, x, z);
        }
    }
    return NO_HEIGHT;
}

static void M_Control(const int16_t item_num)
{
    if (!Creature_Activate(item_num)) {
        return;
    }

    ITEM *const item = Item_Get(item_num);
    CREATURE *const creature = item->creature_data;

    if (item->hit_points <= 0) {
        if (item->current_anim_state != M_STATE_DEATH) {
            Item_SwitchToAnim(item, M_ANIM_DEATH, 0);
            item->current_anim_state = M_STATE_DEATH;
            Carrier_TestItemDrops(item_num);
        }
        Creature_Float(item_num);
        return;
    }

    AI_INFO info;
    Creature_AIInfo(item, &info);
    Creature_Mood(item, &info, false);

    bool shoot;
    const ITEM *const lara_item = Lara_GetItem();
    const LARA_INFO *const lara = Lara_GetLaraInfo();

    if (lara->water_status == LWS_ABOVE_WATER) {
        const GAME_VECTOR start = {
            .x = item->pos.x,
            .y = item->pos.y - STEP_L,
            .z = item->pos.z,
            .room_num = item->room_num,
        };
        GAME_VECTOR target = {
            .x = lara_item->pos.x,
            .y = lara_item->pos.y - (LARA_HEIGHT - 150),
            .z = lara_item->pos.z,
            .room_num = lara_item->room_num,
        };
        shoot = LOS_Check(&start, &target, true);

        if (shoot) {
            creature->target = lara_item->pos;
        }

        if (info.angle < -M_FRONT_ARC || info.angle > M_FRONT_ARC) {
            shoot = false;
        }
    } else if (info.angle > -M_FRONT_ARC && info.angle < M_FRONT_ARC) {
        const GAME_VECTOR start = {
            .x = item->pos.x,
            .y = item->pos.y,
            .z = item->pos.z,
            .room_num = item->room_num,
        };
        GAME_VECTOR target = {
            .x = lara_item->pos.x,
            .y = lara_item->pos.y,
            .z = lara_item->pos.z,
            .room_num = lara_item->room_num,
        };
        shoot = LOS_Check(&start, &target, true);
    } else {
        shoot = false;
    }

    int16_t head = 0;
    int16_t neck = 0;
    int16_t angle = Creature_Turn(item, creature->maximum_turn);
    int32_t water_level =
        M_GetWaterSurface(item->pos.x, item->pos.y, item->pos.z, item->room_num)
        + STEP_L * 2;

    switch (item->current_anim_state) {
    case M_STATE_SWIM_1:
        creature->maximum_turn = M_SWIM_TURN;
        if (shoot) {
            neck = -info.angle;
        }
        if (creature->target.y < water_level
            && item->pos.y < water_level + creature->lot.setup.fly) {
            item->goal_anim_state = M_STATE_SWIM_2;
        } else if (creature->mood != MOOD_ESCAPE && shoot) {
            item->goal_anim_state = M_STATE_AIM_1;
        }
        break;

    case M_STATE_SWIM_2:
        creature->maximum_turn = M_SWIM_TURN;
        if (shoot) {
            head = info.angle;
        }
        if (creature->target.y > water_level) {
            item->goal_anim_state = M_STATE_SWIM_1;
        } else if (creature->mood != MOOD_ESCAPE && shoot) {
            item->goal_anim_state = M_STATE_AIM_2;
        }
        break;

    case M_STATE_SHOOT_1:
        if (shoot) {
            neck = -info.angle;
        }
        if (creature->flags == 0) {
            Creature_Effect(item, &m_DiverBite, Spawn_Harpoon);
            creature->flags = 1;
        }
        break;

    case M_STATE_SHOOT_2:
        if (shoot) {
            head = info.angle;
        }
        if (creature->flags == 0) {
            Creature_Effect(item, &m_DiverBite, Spawn_Harpoon);
            creature->flags = 1;
        }
        break;

    case M_STATE_AIM_1:
        creature->flags = 0;
        if (shoot) {
            neck = -info.angle;
        }
        if (!shoot || creature->mood == MOOD_ESCAPE
            || (creature->target.y < water_level
                && item->pos.y < water_level + creature->lot.setup.fly)) {
            item->goal_anim_state = M_STATE_SWIM_1;
        } else {
            item->goal_anim_state = M_STATE_SHOOT_1;
        }
        break;

    case M_STATE_AIM_2:
        creature->flags = 0;
        if (shoot) {
            head = info.angle;
        }
        if (!shoot || creature->mood == MOOD_ESCAPE
            || creature->target.y > water_level) {
            item->goal_anim_state = M_STATE_SWIM_2;
        } else {
            item->goal_anim_state = M_STATE_SHOOT_2;
        }
        break;

    default:
        break;
    }

    Creature_Head(item, head);
    Creature_Neck(item, neck);

    Creature_Animate(item_num, angle, 0);

    switch (item->current_anim_state) {
    case M_STATE_SWIM_1:
    case M_STATE_AIM_1:
    case M_STATE_SHOOT_1:
        Creature_Underwater(item, WALL_L / 2);
        break;

    default:
        item->pos.y = water_level - WALL_L / 2;
        break;
    }
}

static void M_Setup(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }

    obj->control_func = M_Control;
    obj->collision_func = Creature_Collision;

    obj->hit_points = M_HITPOINTS;
    obj->radius = M_RADIUS;
    obj->shadow_size = UNIT_SHADOW / 2;
    obj->pivot_length = 50;
    obj->lot_setup = LOT_Setup(LOT_SETUP_FLYER);

    obj->intelligent = true;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_flags = true;
    obj->save_anim = true;

    if (g_TRVersion < 3) {
        Object_GetBone(obj, 10)->rot.y = true;
        Object_GetBone(obj, 14)->rot.z = true;
    }
}

REGISTER_OBJECT(O_DIVER, M_Setup)
