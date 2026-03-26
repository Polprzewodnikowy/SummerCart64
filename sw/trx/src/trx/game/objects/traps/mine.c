#include <trx/core/utils.h>
#include <trx/game/effects.h>
#include <trx/game/lara.h>
#include <trx/game/objects/common.h>
#include <trx/game/random.h>
#include <trx/game/rooms.h>
#include <trx/game/sound.h>
#include <trx/game/spawn.h>

static bool m_DetonateAllMines = false;

static int16_t M_GetBoatItem(const XYZ_32 *const pos, int16_t *const room_num)
{
    Room_GetSector(*pos, room_num);

    int16_t item_num = Room_Get(*room_num)->item_num;
    while (item_num != NO_ITEM) {
        const ITEM *const item = Item_Get(item_num);
        if (item->object_id == O_BOAT) {
            const int32_t dx = item->pos.x - pos->x;
            const int32_t dz = item->pos.z - pos->z;
            // TODO: fix overflows and no y check
            if (SQUARE(dx) + SQUARE(dz) < SQUARE(WALL_L / 2)) {
                break;
            }
        }
        item_num = item->next_item;
    }
    return item_num;
}

static void M_DetonateAll(
    const ITEM *const mine_item, const int16_t boat_item_num,
    int16_t boat_room_num)
{
    ITEM *const boat_item = Item_Get(boat_item_num);
    if (Lara_Vehicle_GetIndex() == boat_item_num) {
        ITEM *const lara_item = Lara_GetItem();
        Item_Explode(Item_GetIndex(lara_item), -1, 0);
        lara_item->hit_points = 0;
        lara_item->flags |= IF_ONE_SHOT;
    }

    const OBJECT *const obj = Object_Get(O_BOAT_BITS);
    if (obj->loaded) {
        boat_item->object_id = O_BOAT_BITS;
        boat_item->mesh_bits = (1 << obj->mesh_count) - 1;
        Item_Explode(boat_item_num, -1, 0);
    }
    Item_Kill(boat_item_num);
    boat_item->object_id = O_BOAT;

    Room_TestTriggers(mine_item);

    m_DetonateAllMines = true;
}

static void M_Explode(ITEM *const mine_item)
{
    const int16_t effect_num = Effect_Create(mine_item->room_num);
    if (effect_num != NO_EFFECT) {
        EFFECT *const effect = Effect_Get(effect_num);
        effect->object_id = O_EXPLOSION_1;
        effect->pos.x = mine_item->pos.x;
        effect->pos.y = mine_item->pos.y - WALL_L;
        effect->pos.z = mine_item->pos.z;
        effect->speed = 0;
        effect->frame_num = 0;
        effect->counter = 0;
    }

    Spawn_Splash(mine_item);
    Sound_Effect(SFX_EXPLOSION_1, &mine_item->pos, SPM_NORMAL);

    mine_item->flags |= IF_ONE_SHOT;
    mine_item->mesh_bits = 1;
    mine_item->collidable = false;
}

static void M_HandleSave(ITEM *const item, const SAVEGAME_STAGE stage)
{
    if (stage == SAVEGAME_STAGE_AFTER_LOAD) {
        if (item->flags & IF_ONE_SHOT) {
            item->mesh_bits = 1;
        }
    }
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    if (item->flags & IF_ONE_SHOT) {
        return;
    }

    if (!m_DetonateAllMines) {
        int16_t boat_room_num = item->room_num;
        XYZ_32 test_pos = {
            .x = item->pos.x,
            .y = item->pos.y - WALL_L * 2,
            .z = item->pos.z,
        };
        const int16_t boat_item_num = M_GetBoatItem(&test_pos, &boat_room_num);
        if (boat_item_num == NO_ITEM) {
            return;
        }

        M_DetonateAll(item, boat_item_num, boat_room_num);
    } else if (Random_GetControl() < 0x7800) {
        return;
    }

    M_Explode(item);
}

static void M_Setup(OBJECT *const obj)
{
    obj->handle_save_func = M_HandleSave;
    obj->control_func = M_Control;
    obj->collision_func = Object_Collision;
    obj->save_flags = true;
    obj->enable_interpolation = false;

    m_DetonateAllMines = false;
}

REGISTER_OBJECT(O_MINE, M_Setup)
