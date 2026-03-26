#include <trx/game/creature.h>
#include <trx/game/lara.h>
#include <trx/game/lara/common.h>
#include <trx/game/objects/common.h>
#include <trx/game/objects/traps/common.h>
#include <trx/game/spawn.h>

#define M_DAMAGE 50

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);

    if (!Item_IsTriggerActive(item) && Item_TestFrameEqual(item, -1)) {
        Item_SwitchToAnim(item, 0, 0);
        item->status = IS_INACTIVE;
        Item_RemoveActive(item_num);
        item->enable_interpolation = false;
        return;
    }

    item->enable_interpolation = true;
    if (item->touch_bits != 0) {
        const ITEM *const lara_item = Lara_GetItem();
        Lara_TakeDamage(M_DAMAGE, true);
        Spawn_BloodBath(
            lara_item->pos.x, lara_item->pos.y - WALL_L / 2, lara_item->pos.z,
            lara_item->speed, lara_item->rot.y, lara_item->room_num, 3);
    }

    Item_Animate(item);
}

static void M_HandleSave(ITEM *const item, const SAVEGAME_STAGE stage)
{
    if (stage == SAVEGAME_STAGE_AFTER_LOAD) {
        item->enable_interpolation = item->status == IS_ACTIVE;
    }
}

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = M_Control;
    obj->collision_func = Creature_Collision;
    obj->handle_save_func = M_HandleSave;
    obj->save_flags = true;
    obj->save_anim = true;
}

REGISTER_OBJECT(O_HOOK, M_Setup)
