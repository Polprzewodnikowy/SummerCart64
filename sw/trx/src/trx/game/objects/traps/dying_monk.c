#include <trx/core/utils.h>
#include <trx/game/game.h>
#include <trx/game/lara/common.h>
#include <trx/game/objects.h>
#include <trx/game/rooms.h>

#define MAX_ROOMIES 2

typedef struct {
    int32_t roomies[MAX_ROOMIES];
} M_PRIV;

static void M_Initialise(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;
    for (int32_t i = 0; i < MAX_ROOMIES; i++) {
        p->roomies[i] = NO_ITEM;
    }

    int32_t roomie_count = 0;
    int16_t test_item_num = Room_Get(item->room_num)->item_num;
    while (test_item_num != NO_ITEM) {
        const ITEM *const test_item = Item_Get(test_item_num);
        const OBJECT *const test_obj = Object_Get(test_item->object_id);
        if (test_obj->intelligent) {
            p->roomies[roomie_count++] = test_item_num;
            if (roomie_count >= MAX_ROOMIES) {
                break;
            }
        }
        test_item_num = test_item->next_item;
    }
}

static void M_Control(const int16_t item_num)
{
    const ITEM *const item = Item_Get(item_num);
    const M_PRIV *const p = item->priv;

    for (int32_t i = 0; i < MAX_ROOMIES; i++) {
        int32_t test_item_num = p->roomies[i];
        if (test_item_num != NO_ITEM) {
            const ITEM *const test_item = Item_Get(test_item_num);
            if (test_item->hit_points > 0) {
                return;
            }
        }
    }

    const ITEM *const lara_item = Lara_GetItem();
    const int32_t dx = ABS(lara_item->pos.x - item->pos.x);
    const int32_t dz = ABS(lara_item->pos.z - item->pos.z);
    if (dx < WALL_L && dz < WALL_L && !lara_item->gravity
        && lara_item->pos.y == item->pos.y) {
        Game_SetIsLevelComplete(true);
    }
}

static void M_Setup(OBJECT *const obj)
{
    obj->initialise_func = M_Initialise;
    obj->control_func = M_Control;
    obj->collision_func = Object_Collision;
    obj->priv_size = sizeof(M_PRIV);
    obj->save_flags = true;
}

REGISTER_OBJECT(O_DYING_MONK, M_Setup)
