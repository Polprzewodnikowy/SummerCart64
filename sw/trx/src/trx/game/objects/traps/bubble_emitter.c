#include <trx/game/effects.h>
#include <trx/game/objects.h>
#include <trx/game/random.h>
#include <trx/game/sound.h>
#include <trx/game/spawn.h>

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    if (!Item_IsTriggerActive(item)) {
        item->status = IS_INACTIVE;
        Item_RemoveActive(item_num);
        return;
    }

    if (Random_GetControl() % 24) {
        return;
    }
    const int32_t count = 1 + Random_GetControl() % 2;
    for (int32_t i = 0; i < count; i++) {
        Spawn_Bubble(&item->pos, item->room_num);
    }
}

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = M_Control;
    obj->collision_func = nullptr;
    obj->draw_func = nullptr;
    obj->save_flags = true;
}

REGISTER_OBJECT(O_BUBBLE_EMITTER, M_Setup)
