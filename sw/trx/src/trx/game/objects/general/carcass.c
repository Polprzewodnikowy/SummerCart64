#include <trx/core/utils.h>
#include <trx/game/fx/water.h>
#include <trx/game/objects.h>
#include <trx/game/rooms.h>

// clang-format off
#define M_ROLL_SHIFT       1
#define M_ROLL_SHIFT_UW    3
#define M_MAX_ROLL         0x6000
#define M_ACCEL            8
#define M_ACCEL_UW         1
#define M_MAX_SPEED        (STEP_L * 2)  // = 512
#define M_MAX_SPEED_UW     (STEP_L / 4)  // = 64
#define M_INITIAL_SPEED_UW (STEP_L / 16) // = 16
// clang-format on

static void M_SpawnSplash(const ITEM *const item)
{
    const ROOM *const room = Room_Get(item->room_num);
    const FX_WATER_SPLASH_SETUP setup = {
        .x = item->pos.x,
        .y = room->max_ceiling,
        .z = item->pos.z,
        .inner_xz_off = 16,
        .inner_xz_size = 16,
        .inner_y_size = -96,
        .inner_xz_vel = 160,
        .inner_y_vel = (int16_t)(-72 * item->fall_speed),
        .inner_gravity = 128,
        .inner_friction = 7,
        .middle_xz_off = 24,
        .middle_xz_size = 32,
        .middle_y_size = -64,
        .middle_xz_vel = 224,
        .middle_y_vel = (int16_t)(-36 * item->fall_speed),
        .middle_gravity = 72,
        .middle_friction = 8,
        .outer_xz_off = 32,
        .outer_xz_size = 32,
        .outer_xz_vel = 272,
        .outer_friction = 9,
    };
    FX_Water_SetupSplash(&setup);
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    if (item->status != IS_ACTIVE) {
        return;
    }

    item->pos.y += item->fall_speed;

    const bool was_underwater = Room_Get(item->room_num)->flags.underwater;
    int16_t room_num = item->room_num;
    const SECTOR *const sector = Room_GetSector(item->pos, &room_num);
    Item_UpdateRoom(item_num, room_num);

    const int16_t height = Room_GetHeight(sector, item->pos) - M_MAX_SPEED_UW;
    if (item->pos.y >= height) {
        item->pos.y = height;
        item->fall_speed = 0;
        // TODO: this is tied to the slope the carcass lands on in Crash Site
        item->rot.z = M_MAX_ROLL;
        return;
    }

    const ROOM *const current_room = Room_Get(room_num);
    const bool is_underwater = current_room->flags.underwater;
    item->rot.z += item->fall_speed
        << (is_underwater ? M_ROLL_SHIFT_UW : M_ROLL_SHIFT);
    item->fall_speed += is_underwater ? M_ACCEL_UW : M_ACCEL;
    CLAMPG(item->rot.z, M_MAX_ROLL);
    CLAMPG(item->fall_speed, was_underwater ? M_MAX_SPEED_UW : M_MAX_SPEED);

    if (is_underwater && !was_underwater) {
        M_SpawnSplash(item);
        item->fall_speed = M_INITIAL_SPEED_UW;
        item->pos.y = current_room->max_ceiling + 1;
    }
}

static void M_Setup(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }

    obj->control_func = M_Control;
    obj->collision_func = Object_Collision;
    obj->save_position = true;
    obj->save_flags = true;
    obj->save_anim = true;
}

REGISTER_OBJECT(O_CARCASS, M_Setup)
