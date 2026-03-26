#include <trx/game/objects/general/smashable.h>

#include <trx/core/math.h>
#include <trx/core/utils.h>
#include <trx/game/lara.h>
#include <trx/game/objects/common.h>
#include <trx/game/pathing.h>
#include <trx/game/rooms.h>
#include <trx/game/sound.h>

static void M_SetBoxBlocked(const ITEM *const item, const bool blocked)
{
    const ROOM *const room = Room_Get(item->room_num);
    const SECTOR *const sector =
        Room_GetWorldSector(room, item->pos.x, item->pos.z);
    BOX_INFO *const box = Box_GetBox(sector->box);

    if (blocked && (box->overlap_index & BOX_BLOCKABLE) != 0) {
        box->overlap_index |= BOX_BLOCKED;
    } else if (!blocked && (box->overlap_index & BOX_BLOCKED) != 0) {
        box->overlap_index &= ~BOX_BLOCKED;
    }
}

static void M_Initialise(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    item->flags = 0;
    item->mesh_bits = 1;
    M_SetBoxBlocked(item, true);
}

static void M_HandleSave(ITEM *const item, const SAVEGAME_STAGE stage)
{
    if (stage == SAVEGAME_STAGE_AFTER_LOAD) {
        if ((item->flags & IF_ONE_SHOT) != 0) {
            item->mesh_bits = 0x100;
            M_SetBoxBlocked(item, false);
        }
    }
}

static void M_Control1(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    if ((item->flags & IF_ONE_SHOT) != 0) {
        return;
    }

    if (Lara_Vehicle_IsMounted()) {
        if (Lara_IsNearItem(&item->pos, 512)) {
            Smashable_Smash(item_num);
        }
    } else if (item->touch_bits) {
        item->touch_bits = 0;
        const ITEM *const lara_item = Lara_GetItem();
        const int32_t speed =
            ABS((lara_item->speed * Math_Cos(lara_item->rot.y - item->rot.y))
                >> W2V_SHIFT);
        if (speed >= 50) {
            Smashable_Smash(item_num);
        }
    }
}

static void M_Control2(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    if ((item->flags & IF_ONE_SHOT) != 0) {
        return;
    }

    M_SetBoxBlocked(item, false);

    item->mesh_bits = ~1;
    item->collidable = false;
    Item_Explode(item_num, 65278, 0);

    if (item->object_id == O_SMASH_OBJECT_2) {
        Sound_Effect(SFX_BRITTLE_GROUND_BREAK, &item->pos, SPM_NORMAL);
    } else if (item->object_id == O_SMASH_OBJECT_3) {
        Sound_Effect(SFX_EXPLOSION_1, &item->pos, SPM_NORMAL);
        Sound_Effect(SFX_EXPLOSION_2, &item->pos, SPM_NORMAL);
    }

    item->flags |= IF_ONE_SHOT;
    item->status = IS_DEACTIVATED;
    Item_RemoveActive(item_num);
}

static void M_SetupBase(OBJECT *const obj)
{
    obj->initialise_func = M_Initialise;
    obj->handle_save_func = M_HandleSave;
    obj->collision_func = Object_Collision;
    obj->save_flags = true;
    obj->save_anim = true;
}

static void M_Setup1(OBJECT *const obj)
{
    M_SetupBase(obj);
    obj->control_func = M_Control1;
}

static void M_Setup2(OBJECT *const obj)
{
    M_SetupBase(obj);
    obj->control_func = M_Control2;
}

void Smashable_Smash(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    M_SetBoxBlocked(item, false);

    item->collidable = false;
    item->mesh_bits = ~1;
    Item_Explode(item_num, 0b11111110'11111110, 0);

    if (item->object_id == O_SMASH_OBJECT_1) {
        Sound_Effect(SFX_GLASS_BREAK, &item->pos, SPM_NORMAL);
    } else if (item->object_id == O_SMASH_OBJECT_4) {
        Sound_Effect(SFX_SHUTTERS_BREAK, &item->pos, SPM_NORMAL);
    }

    item->flags |= IF_ONE_SHOT;
    if (item->status == IS_ACTIVE) {
        Item_RemoveActive(item_num);
    }
    item->status = IS_DEACTIVATED;
}

REGISTER_OBJECT(O_SMASH_OBJECT_1, M_Setup1)
REGISTER_OBJECT(O_SMASH_OBJECT_2, M_Setup2)
REGISTER_OBJECT(O_SMASH_OBJECT_3, M_Setup2)
REGISTER_OBJECT(O_SMASH_OBJECT_4, M_Setup1)
