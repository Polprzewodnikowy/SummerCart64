#include <trx/game/items/manager.h>

#include <trx/core/memory.h>
#include <trx/core/utils.h>
#include <trx/game/game.h>
#include <trx/game/game_buf.h>
#include <trx/game/game_flow.h>
#include <trx/game/items/carrier.h>
#include <trx/game/lara/common.h>
#include <trx/game/objects.h>
#include <trx/game/objects/general/shoal.h>
#include <trx/game/output/const.h>
#include <trx/game/rooms.h>
#include <trx/game/sparks.h>
#include <trx/version.h>

#include <string.h>

static int32_t m_LevelItemCount = 0;
static int16_t m_MaxUsedItemCount = 0;
static ITEM *m_Items = nullptr;
static int16_t m_NextItemActive = NO_ITEM;
static int16_t m_PrevItemActive = NO_ITEM;
static int16_t m_NextItemFree = NO_ITEM;

static inline bool M_ItemBoundsIntersectsPortal(
    const ITEM *item, const ROOM *room, const PORTAL *const portal)
{
    // Axis-aligned bound intersection; ignores item rotation.
    const BOUNDS_16 *const frame_bounds = &Item_GetBestFrame(item)->bounds;
    if (frame_bounds == nullptr) {
        return false;
    }
    const BOUNDS_32 bounds = {
        .min = {
            item->pos.x + frame_bounds->min.x,
            item->pos.y + frame_bounds->min.y,
            item->pos.z + frame_bounds->min.z,
        },
        .max = {
            item->pos.x + frame_bounds->max.x,
            item->pos.y + frame_bounds->max.y,
            item->pos.z + frame_bounds->max.z,
        },
    };

    if (Bounds32_Intersect(&bounds, &portal->bounds)) {
        return true;
    }

    const ROOM *const own_room = Room_Get(item->room_num);
    if (own_room == nullptr) {
        return false;
    }
    const BOUNDS_32 room_bounds = Room_GetRoomBounds(own_room);
    return !Bounds32_Intersect(&bounds, &room_bounds);
}

void Item_InitialiseItems(const int32_t num_items)
{
    m_Items = GameBuf_Alloc(sizeof(ITEM) * MAX_ITEMS, GBUF_ITEMS);
    m_LevelItemCount = num_items;
    m_MaxUsedItemCount = num_items;
    m_NextItemFree = num_items;
    m_NextItemActive = NO_ITEM;
    m_PrevItemActive = NO_ITEM;

    for (int32_t i = m_NextItemFree; i < MAX_ITEMS - 1; i++) {
        ITEM *const item = &m_Items[i];
        item->active = false;
        item->next_item = i + 1;
    }
    m_Items[MAX_ITEMS - 1].next_item = NO_ITEM;
}

ITEM *Item_Get(const int16_t item_num)
{
    if (item_num == NO_ITEM) {
        return nullptr;
    }
    return &m_Items[item_num];
}

int16_t Item_GetIndex(const ITEM *const item)
{
    return item - Item_Get(0);
}

ITEM *Item_Find(const OBJECT_ID obj_id)
{
    for (int32_t item_num = 0; item_num < Item_GetTotalCount(); item_num++) {
        ITEM *const item = Item_Get(item_num);
        if (item->object_id == obj_id) {
            return item;
        }
    }

    return nullptr;
}

bool Item_SetName(const int16_t item_num, const char *const name)
{
    ITEM *const item = Item_Get(item_num);
    if (item == nullptr) {
        return false;
    }
    if (name != nullptr) {
        ITEM *const existing = Item_GetByName(name);
        if (existing != nullptr && existing != item) {
            return false;
        }
    }
    if (name != nullptr) {
        item->name = GameBuf_Alloc(strlen(name) + 1, GBUF_ITEMS);
        strcpy(item->name, name);
    } else {
        item->name = nullptr;
    }
    return true;
}

ITEM *Item_GetByName(const char *const name)
{
    if (name == nullptr) {
        return nullptr;
    }
    // search through all items for matching name
    for (int32_t i = 0; i < Item_GetTotalCount(); i++) {
        ITEM *const item = Item_Get(i);
        if (item->name != nullptr && strcmp(item->name, name) == 0) {
            return item;
        }
    }
    return nullptr;
}

int32_t Item_GetLevelCount(void)
{
    return m_LevelItemCount;
}

int32_t Item_GetTotalCount(void)
{
    return m_MaxUsedItemCount;
}

int16_t Item_GetNextActive(void)
{
    return m_NextItemActive;
}

int16_t Item_GetPrevActive(void)
{
    return m_PrevItemActive;
}

void Item_SetPrevActive(const int16_t item_num)
{
    m_PrevItemActive = item_num;
}

int16_t Item_Create(void)
{
    const int16_t item_num = m_NextItemFree;
    if (item_num != NO_ITEM) {
        m_Items[item_num].flags = 0;
        m_NextItemFree = m_Items[item_num].next_item;
    }
    m_MaxUsedItemCount = MAX(m_MaxUsedItemCount, item_num + 1);
    return item_num;
}

int16_t Item_CreateLevelItem(void)
{
    const int16_t item_num = Item_Create();
    if (item_num != NO_ITEM) {
        m_LevelItemCount++;
    }
    return item_num;
}

int16_t Item_Spawn(const ITEM *const item, const OBJECT_ID obj_id)
{
    const int16_t spawn_num = Item_Create();
    if (spawn_num != NO_ITEM) {
        ITEM *const spawn = Item_Get(spawn_num);
        spawn->object_id = obj_id;
        spawn->room_num = item->room_num;
        spawn->pos = item->pos;
        spawn->rot = item->rot;
        Item_Initialise(spawn_num);
        spawn->status = IS_INACTIVE;
        spawn->shade.value_1 = SHADE_NEUTRAL;
    }
    return spawn_num;
}

void Item_Initialise(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    const OBJECT *const obj = Object_Get(item->object_id);

    Item_SwitchToAnim(item, 0, 0);
    item->goal_anim_state = Item_GetAnim(item)->current_anim_state;
    item->current_anim_state = item->goal_anim_state;
    item->required_anim_state = 0;
    item->rot.x = 0;
    item->rot.z = 0;
    item->speed = 0;
    item->fall_speed = 0;
    item->hit_points = obj->hit_points;
    item->max_hit_points = obj->hit_points;
    item->timer = 0;
    item->mesh_bits = 0xFFFFFFFF;
    item->touch_bits = 0;
    item->ai_bits = 0;
    item->ai_tag = 0;
    item->after_death = 0;
    item->creature_data = nullptr;
    item->extra_rotations = nullptr;
    item->priv = nullptr;
    item->carried_item = nullptr;
    item->name = nullptr;

    item->active = false;
    item->status = IS_INACTIVE;
    item->gravity = false;
    item->hit_status = false;
    item->collidable = true;
    item->looked_at = false;
    item->enable_interpolation = true;
    item->enable_shadow = true;
    item->dynamic_light = false;
    item->include_in_kill_stats = true;

    item->clear_body = false;
    if ((item->flags & IF_KILLED) != 0) {
        item->clear_body = true;
        item->flags &= ~IF_KILLED;
    }

    if ((item->flags & IF_INVISIBLE) != 0) {
        item->status = IS_INVISIBLE;
        item->flags &= ~IF_INVISIBLE;
    } else if (g_TRVersion >= 2 && obj->intelligent) {
        item->status = IS_INVISIBLE;
    }

    if ((item->flags & IF_CODE_BITS) == IF_CODE_BITS) {
        item->flags &= ~IF_CODE_BITS;
        item->flags |= IF_REVERSE;
        Item_AddActive(item_num);
        item->status = IS_ACTIVE;
    }

    ROOM *const room = Room_Get(item->room_num);
    item->next_item = room->item_num;
    room->item_num = item_num;

    const SECTOR *const sector =
        Room_GetWorldSector(room, item->pos.x, item->pos.z);
    item->floor = sector->floor.height;

    // TODO: remove GF check once demo config reset is run before level load
    if (Game_IsBonusFlagSet(GBF_NGPLUS)
        && GF_GetCurrentLevel()->type != GFL_DEMO) {
        item->hit_points *= 2;
    }

    if (obj->priv_size != 0) {
        if (item->priv != nullptr) {
            memset(item->priv, 0, obj->priv_size);
        } else {
            item->priv = GameBuf_Alloc(obj->priv_size, GBUF_ITEM_DATA);
        }
    }

    if (obj->initialise_func != nullptr) {
        obj->initialise_func(item_num);
    }

    if (item->room_num != NO_ROOM) {
        Room_AddDrawnItem(item->room_num, item_num);
    }
}

void Item_Control(void)
{
    int16_t item_num = Item_GetNextActive();
    while (item_num != NO_ITEM) {
        const ITEM *const item = Item_Get(item_num);
        const int16_t next = item->next_active;
        const OBJECT *obj = Object_Get(item->object_id);
        if ((item->flags & IF_KILLED) == 0 && obj->control_func != nullptr) {
            obj->control_func(item_num);
        }
        item_num = next;
    }

    Carrier_AnimateDrops();
}

void Item_Kill(const int16_t item_num)
{
    Sparks_DetachItem(item_num);
    Item_RemoveActive(item_num);
    Item_RemoveDrawn(item_num);

    ITEM *const item = &m_Items[item_num];
    LARA_INFO *const lara = Lara_GetLaraInfo();
    if (item == lara->target) {
        lara->target = nullptr;
    }

    item->flags |= IF_KILLED;

    if (item_num >= m_LevelItemCount) {
        item->next_item = m_NextItemFree;
        m_NextItemFree = item_num;
    }

    while (m_MaxUsedItemCount > 0
           && m_Items[m_MaxUsedItemCount - 1].flags & IF_KILLED) {
        m_MaxUsedItemCount--;
    }
}

void Item_KillAllActive(void)
{
    int16_t item_num = Item_GetNextActive();
    while (item_num != NO_ITEM) {
        ITEM *const item = Item_Get(item_num);
        const int16_t next_item_num = item->next_active;

        if (item->active && (item->flags & IF_REVERSE) == 0
            && item->object_id != O_LARA
            && item->object_id != O_SAVE_CRYSTAL_ITEM
            && !Object_IsType(item->object_id, g_PickupObjects)
            && !Object_IsType(item->object_id, g_DoorObjects)) {
            Item_Kill(item_num);

            if (Object_IsType(item->object_id, g_ShoalObjects)) {
                Shoal_TriggerDeactivate(item);
            }
        }
        item_num = next_item_num;
    }
}

void Item_RemoveActive(const int16_t item_num)
{
    ITEM *const item = &m_Items[item_num];
    if (!item->active) {
        return;
    }

    item->active = false;

    int16_t link_num = m_NextItemActive;
    if (link_num == item_num) {
        m_NextItemActive = item->next_active;
        return;
    }

    while (link_num != NO_ITEM) {
        if (m_Items[link_num].next_active == item_num) {
            m_Items[link_num].next_active = item->next_active;
            return;
        }
        link_num = m_Items[link_num].next_active;
    }
}

void Item_RemoveDrawn(const int16_t item_num)
{
    const ITEM *const item = &m_Items[item_num];
    if (item->room_num == NO_ROOM) {
        return;
    }

    ROOM *const room = Room_Get(item->room_num);
    Room_RemoveDrawnItem(item->room_num, item_num);
    if (room->portals != nullptr) {
        for (int32_t i = 0; i < room->portals->count; i++) {
            const PORTAL *const portal = &room->portals->portal[i];
            Room_RemoveDrawnItem(portal->room_num, item_num);
        }
    }

    int16_t link_num = room->item_num;
    if (link_num == item_num) {
        room->item_num = item->next_item;
        return;
    }

    while (link_num != NO_ITEM) {
        if (m_Items[link_num].next_item == item_num) {
            m_Items[link_num].next_item = item->next_item;
            return;
        }
        link_num = m_Items[link_num].next_item;
    }
}

void Item_ClearKilled(void)
{
    // Remove corpses and other killed items. Part of OG performance
    // improvements, generously used in Opera House and Barkhang Monastery
    int16_t link_num = Item_GetPrevActive();
    while (link_num != NO_ITEM) {
        ITEM *const item = Item_Get(link_num);
        Item_Kill(link_num);
        link_num = item->next_active;
        item->next_active = NO_ITEM;
    }
    Item_SetPrevActive(NO_ITEM);
}

void Item_AddActive(const int16_t item_num)
{
    ITEM *const item = &m_Items[item_num];
    if (Object_Get(item->object_id)->control_func == nullptr) {
        item->status = IS_INACTIVE;
        return;
    }

    if (item->active) {
        return;
    }

    item->active = true;
    item->next_active = m_NextItemActive;
    m_NextItemActive = item_num;
}

void Item_UpdateRoom(const int16_t item_num, const int16_t room_num)
{
    ITEM *const item = &m_Items[item_num];
    const int16_t old_room_num = item->room_num;

    // Add to new-room draw queues (including portal rooms)
    // draw queue removal for primary room and portal rooms
    if (old_room_num != NO_ROOM) {
        Room_RemoveDrawnItem(old_room_num, item_num);
        const ROOM *const room = Room_Get(old_room_num);
        if (room != nullptr && room->portals != nullptr) {
            for (int32_t i = 0; i < room->portals->count; i++) {
                Room_RemoveDrawnItem(
                    room->portals->portal[i].room_num, item_num);
            }
        }
    }
    if (room_num != NO_ROOM) {
        Room_AddDrawnItem(room_num, item_num);
        const ROOM *const neighbor_room = Room_Get(room_num);
        if (neighbor_room != nullptr && neighbor_room->portals != nullptr) {
            for (int32_t i = 0; i < neighbor_room->portals->count; i++) {
                const PORTAL *const portal = &neighbor_room->portals->portal[i];
                if (M_ItemBoundsIntersectsPortal(item, neighbor_room, portal)) {
                    Room_AddDrawnItem(portal->room_num, item_num);
                }
            }
        }
    }

    if (old_room_num != room_num) {
        ROOM *room = nullptr;
        if (old_room_num != NO_ROOM) {
            room = Room_Get(old_room_num);
            int16_t link_num = room->item_num;
            if (link_num == item_num) {
                room->item_num = item->next_item;
            } else {
                while (link_num != NO_ITEM) {
                    if (m_Items[link_num].next_item == item_num) {
                        m_Items[link_num].next_item = item->next_item;
                        break;
                    }
                    link_num = m_Items[link_num].next_item;
                }
            }
        }

        if (room_num == NO_ROOM) {
            Item_RemoveDrawn(item_num);
            item->room_num = NO_ROOM;
        } else {
            room = Room_Get(room_num);
            item->room_num = room_num;
            item->next_item = room->item_num;
            room->item_num = item_num;
        }
    }
}

int32_t Item_GlobalReplace(
    const OBJECT_ID src_obj_id, const OBJECT_ID dst_obj_id)
{
    int32_t changed = 0;

    for (int32_t item_num = 0; item_num < m_MaxUsedItemCount; item_num++) {
        ITEM *const item = &m_Items[item_num];
        if (item->object_id == src_obj_id) {
            item->object_id = dst_obj_id;
            changed++;
        }
    }

    return changed;
}
