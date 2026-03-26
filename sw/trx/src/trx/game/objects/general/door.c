#include <trx/game/objects/general/door.h>

#include <trx/game/lara.h>
#include <trx/game/objects/common.h>
#include <trx/game/pathing.h>
#include <trx/game/rooms.h>

typedef struct {
    SECTOR *sector;
    SECTOR old_sector;
    int16_t box_num;
} M_DOOR_POS;

typedef struct {
    M_DOOR_POS d1;
    M_DOOR_POS d1flip;
    M_DOOR_POS d2;
    M_DOOR_POS d2flip;
} M_PRIV;

static const SECTOR m_BlockedSector = {
    .idx = 0,
    .box = NO_BOX,
    .ceiling.height = NO_HEIGHT,
    .floor.height = NO_HEIGHT,
    .ceiling.tilt = 0,
    .floor.tilt = 0,
    .portal_room.sky = NO_ROOM,
    .portal_room.pit = NO_ROOM,
    .portal_room.wall = NO_ROOM,
};

static SECTOR *M_GetRoomRelSector(
    const ROOM *const room, const ITEM *item, const int32_t sector_dx,
    const int32_t sector_dz)
{
    const XZ_32 sector = {
        .x = ((item->pos.x - room->pos.x) >> WALL_SHIFT) + sector_dx,
        .z = ((item->pos.z - room->pos.z) >> WALL_SHIFT) + sector_dz,
    };
    return Room_GetUnitSector(room, sector.x, sector.z);
}

static bool M_LaraDoorCollision(const SECTOR *const sector)
{
    // Check if Lara is on the same tile as the invisible block.
    const ITEM *const lara = Lara_GetItem();
    if (lara == nullptr) {
        return false;
    }

    int16_t room_num = lara->room_num;
    const SECTOR *const lara_sector = Room_GetSector(lara->pos, &room_num);
    return lara_sector == sector;
}

static void M_CopySectorProperties(
    const SECTOR *const source_sector, SECTOR *const target_sector)
{
    target_sector->idx = source_sector->idx;
    target_sector->box = source_sector->box;
    target_sector->ceiling.height = source_sector->ceiling.height;
    target_sector->floor.height = source_sector->floor.height;
    target_sector->floor.tilt = source_sector->floor.tilt;
    target_sector->ceiling.tilt = source_sector->ceiling.tilt;
    target_sector->portal_room.sky = source_sector->portal_room.sky;
    target_sector->portal_room.pit = source_sector->portal_room.pit;
    target_sector->portal_room.wall = source_sector->portal_room.wall;
}

static void M_Open(M_DOOR_POS *const d)
{
    if (d->sector == nullptr) {
        return;
    }

    M_CopySectorProperties(&d->old_sector, d->sector);

    const int16_t box_num = d->box_num;
    if (box_num != NO_BOX) {
        Box_GetBox(box_num)->overlap_index &= ~BOX_BLOCKED;
    }
}

static void M_Check(M_DOOR_POS *const d)
{
    // Forcefully remove the invisible block if Lara happens to occupy the same
    // tile. This ensures that Lara doesn't void if a timed door happens to
    // close right on her, or the player loads the game while standing on a
    // closed door's block tile.
    if (M_LaraDoorCollision(d->sector)) {
        M_Open(d);
    }
}

static void M_Shut(M_DOOR_POS *const d)
{
    if (d->sector == nullptr) {
        return;
    }

    M_CopySectorProperties(&m_BlockedSector, d->sector);

    const int16_t box_num = d->box_num;
    if (box_num != NO_BOX) {
        Box_GetBox(box_num)->overlap_index |= BOX_BLOCKED;
    }
}

static void M_InitialisePortal(
    const ROOM *const room, const ITEM *const item, const int32_t sector_dx,
    const int32_t sector_dz, M_DOOR_POS *const door_pos)
{
    door_pos->sector = M_GetRoomRelSector(room, item, sector_dx, sector_dz);

    const SECTOR *sector = door_pos->sector;

    const int16_t room_num = door_pos->sector->portal_room.wall;
    if (room_num != NO_ROOM) {
        sector =
            M_GetRoomRelSector(Room_Get(room_num), item, sector_dx, sector_dz);
    }

    int16_t box_num = sector->box;
    const BOX_INFO *const box = Box_GetBox(box_num);
    if ((box->overlap_index & BOX_BLOCKABLE) == 0) {
        box_num = NO_BOX;
    }
    door_pos->box_num = box_num;
    door_pos->old_sector = *door_pos->sector;
}

static void M_Initialise(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;

    int32_t dx = 0;
    int32_t dz = 0;
    if (item->rot.y == 0) {
        dz = -1;
    } else if (item->rot.y == -DEG_180) {
        dz = 1;
    } else if (item->rot.y == DEG_90) {
        dx = -1;
    } else {
        dx = 1;
    }

    int16_t room_num = item->room_num;
    const ROOM *room = Room_Get(room_num);
    M_InitialisePortal(room, item, dx, dz, &p->d1);

    if (room->flipped_room == NO_ROOM) {
        p->d1flip.sector = nullptr;
    } else {
        room = Room_Get(room->flipped_room);
        M_InitialisePortal(room, item, dx, dz, &p->d1flip);
    }

    room_num = p->d1.sector->portal_room.wall;
    M_Shut(&p->d1);
    M_Shut(&p->d1flip);

    if (room_num == NO_ROOM) {
        p->d2.sector = nullptr;
        p->d2flip.sector = nullptr;
    } else {
        room = Room_Get(room_num);
        M_InitialisePortal(room, item, 0, 0, &p->d2);
        if (room->flipped_room == NO_ROOM) {
            p->d2flip.sector = nullptr;
        } else {
            room = Room_Get(room->flipped_room);
            M_InitialisePortal(room, item, 0, 0, &p->d2flip);
        }

        M_Shut(&p->d2);
        M_Shut(&p->d2flip);

        const int16_t prev_room = item->room_num;
        Item_UpdateRoom(item_num, room_num);
        item->room_num = prev_room;
    }
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;

    if (Item_IsTriggerActive(item)) {
        if (item->current_anim_state == DOOR_STATE_CLOSED) {
            item->goal_anim_state = DOOR_STATE_OPEN;
        } else {
            M_Open(&p->d1);
            M_Open(&p->d2);
            M_Open(&p->d1flip);
            M_Open(&p->d2flip);
        }
    } else {
        if (item->current_anim_state == DOOR_STATE_OPEN) {
            item->goal_anim_state = DOOR_STATE_CLOSED;
        } else {
            M_Shut(&p->d1);
            M_Shut(&p->d2);
            M_Shut(&p->d1flip);
            M_Shut(&p->d2flip);
        }
    }

    M_Check(&p->d1);
    M_Check(&p->d2);
    M_Check(&p->d1flip);
    M_Check(&p->d2flip);
    Item_Animate(item);
}

static void M_Setup(OBJECT *const obj)
{
    obj->initialise_func = M_Initialise;
    obj->control_func = M_Control;
    obj->draw_func = Object_DrawUnclippedItem;
    obj->collision_func = Door_Collision;
    obj->priv_size = sizeof(M_PRIV);
    obj->save_flags = true;
    obj->save_anim = true;
}

void Door_Collision(
    const int16_t item_num, ITEM *const lara_item, COLL_INFO *const coll)
{
    ITEM *const item = Item_Get(item_num);

    if (!Item_TestBoundsCollide(item, lara_item, coll->radius)) {
        return;
    }

    if (!Collide_TestCollision(item, lara_item)) {
        return;
    }

    if (coll->enable_baddie_push) {
        Lara_Col_ItemPush(
            item, coll,
            coll->enable_hit
                && item->current_anim_state != item->goal_anim_state,
            true);
    }
}

REGISTER_OBJECT(O_DOOR_TYPE_1, M_Setup)
REGISTER_OBJECT(O_DOOR_TYPE_2, M_Setup)
REGISTER_OBJECT(O_DOOR_TYPE_3, M_Setup)
REGISTER_OBJECT(O_DOOR_TYPE_4, M_Setup)
REGISTER_OBJECT(O_DOOR_TYPE_5, M_Setup)
REGISTER_OBJECT(O_DOOR_TYPE_6, M_Setup)
REGISTER_OBJECT(O_DOOR_TYPE_7, M_Setup)
REGISTER_OBJECT(O_DOOR_TYPE_8, M_Setup)
