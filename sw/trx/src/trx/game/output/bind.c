#include <trx/game/output/bind.h>

#include <trx/game/items.h>
#include <trx/game/rooms/common.h>
#include <trx/game/rooms/const.h>

#include <string.h>

static OUTPUT_ITEM_BIND m_ItemBindings[MAX_ITEMS] = {};
static OUTPUT_ROOM_BIND m_RoomBindings[MAX_ROOMS] = {};

void Output_Bind_ResetItems(void)
{
    memset(m_ItemBindings, 0, sizeof(m_ItemBindings));
}

OUTPUT_ITEM_BIND *Output_Bind_GetItem(const ITEM *const item)
{
    return &m_ItemBindings[Item_GetIndex(item)];
}

void Output_Bind_ResetRooms(void)
{
    for (int32_t i = 0; i < MAX_ROOMS; i++) {
        m_RoomBindings[i].active = false;
        m_RoomBindings[i].drawn = false;
    }
}

OUTPUT_ROOM_BIND *Output_Bind_GetRoom(const ROOM *const room)
{
    return &m_RoomBindings[Room_GetNumber(room)];
}
