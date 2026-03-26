#include <trx/core/utils.h>
#include <trx/game/lara.h>
#include <trx/game/objects/traps/common.h>
#include <trx/game/random.h>
#include <trx/game/rooms.h>
#include <trx/game/sound.h>
#include <trx/game/spawn.h>

#define M_ACTIVATE_DIST ((WALL_L * 3) / 2)
#define M_DAMAGE 100

static void M_Reset(ITEM *const item)
{
    item->required_anim_state = (Random_GetControl() - 0x4000) / 16;
    item->fall_speed = 50;

    int16_t room_num = item->room_num;
    const SECTOR *const sector = Room_GetSector(
        (XYZ_32) { item->pos.x, MAX_HEIGHT, item->pos.z }, &room_num);
    item->floor = Room_GetHeight(sector, item->pos);
}

static void M_Initialise(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    item->rot.y = Random_GetControl();

    Trap_Initialise(item_num);
    M_Reset(item);
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    if (!Item_IsTriggerActive(item)) {
        Trap_Reset(item);
        M_Reset(item);
        return;
    }

    if (item->status == IS_DEACTIVATED) {
        return;
    }

    if (item->gravity) {
        item->rot.y += item->required_anim_state;
        item->fall_speed += item->fall_speed < FAST_FALL_SPEED ? GRAVITY : 1;
        item->pos.y += item->fall_speed;
        item->pos.x += item->current_anim_state;
        item->pos.z += item->goal_anim_state;

        int16_t room_num = item->room_num;
        const SECTOR *const sector = Room_GetSector(item->pos, &room_num);
        item->floor = Room_GetHeight(sector, item->pos);
        Item_UpdateRoom(item_num, room_num);

        if (item->pos.y > item->floor) {
            Sound_Effect(SFX_DAMOCLES_SWORD, &item->pos, SPM_NORMAL);
            item->pos.y = item->floor + 10;
            item->gravity = false;
            item->status = IS_DEACTIVATED;
        }
    } else if (item->pos.y != item->floor) {
        item->rot.y += item->required_anim_state;
        const ITEM *const lara_item = Lara_GetItem();
        const int32_t x = lara_item->pos.x - item->pos.x;
        const int32_t y = lara_item->pos.y - item->pos.y;
        const int32_t z = lara_item->pos.z - item->pos.z;
        if (ABS(x) <= M_ACTIVATE_DIST && ABS(z) <= M_ACTIVATE_DIST && y > 0
            && y < WALL_L * 3) {
            item->current_anim_state = x / 32;
            item->goal_anim_state = z / 32;
            item->gravity = true;
        }
    }
}

static void M_Collision(
    const int16_t item_num, ITEM *const lara_item, COLL_INFO *const coll)
{
    ITEM *const item = Item_Get(item_num);
    if (!Lara_TestBoundsCollide(item, coll->radius)) {
        return;
    }
    if (coll->enable_baddie_push) {
        Lara_Col_ItemPush(item, coll, false, true);
    }
    if (item->gravity) {
        lara_item->hit_points -= M_DAMAGE;
        int32_t x = lara_item->pos.x + (Random_GetControl() - 0x4000) / 256;
        int32_t z = lara_item->pos.z + (Random_GetControl() - 0x4000) / 256;
        int32_t y = lara_item->pos.y - Random_GetControl() / 44;
        int32_t d = lara_item->rot.y + (Random_GetControl() - 0x4000) / 8;
        Spawn_Blood(x, y, z, lara_item->speed, d, lara_item->room_num);
    }
}

static void M_Setup(OBJECT *const obj)
{
    obj->initialise_func = M_Initialise;
    obj->control_func = M_Control;
    obj->collision_func = M_Collision;
    obj->shadow_size = UNIT_SHADOW;
    obj->save_position = true;
    obj->save_anim = true;
    obj->save_flags = true;
}

REGISTER_OBJECT(O_DAMOCLES_SWORD, M_Setup)
