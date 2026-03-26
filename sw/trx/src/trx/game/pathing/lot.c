#include <trx/game/pathing/lot.h>

#include <trx/core/utils.h>
#include <trx/debug.h>
#include <trx/game/camera.h>
#include <trx/game/game_buf.h>
#include <trx/game/objects.h>
#include <trx/game/pathing.h>
#include <trx/game/rooms.h>

static int32_t m_SlotsUsed = 0;
static CREATURE *m_BaddieSlots = nullptr;

LOT_SETUP LOT_Setup(const LOT_SETUP_TYPE type)
{
    switch (type) {
    case LOT_SETUP_DEFAULT:
        return (LOT_SETUP) {
            .step = STEP_L,
            .drop = g_TRVersion == 1 ? -STEP_L : -STEP_L * 2,
            .fly = 0,
            .block_mask = BOX_BLOCKED,
        };

    case LOT_SETUP_BEAST:
        return (LOT_SETUP) {
            .step = STEP_L,
            .drop = g_TRVersion == 1 ? -STEP_L : -STEP_L * 2,
            .fly = 0,
            .block_mask = BOX_BLOCKABLE,
        };

    case LOT_SETUP_QUADRUPED:
        return (LOT_SETUP) {
            .step = STEP_L,
            .drop = -WALL_L,
            .fly = 0,
            .block_mask = BOX_BLOCKED,
        };

    case LOT_SETUP_JUMPER:
        return (LOT_SETUP) {
            .step = WALL_L / 2,
            .drop = -WALL_L,
            .fly = 0,
            .block_mask = BOX_BLOCKED,
        };

    case LOT_SETUP_CLIMBER:
        return (LOT_SETUP) {
            .step = WALL_L,
            .drop = -WALL_L,
            .fly = 0,
            .block_mask = BOX_BLOCKED,
        };

    case LOT_SETUP_FLYER:
        return (LOT_SETUP) {
            .step = WALL_L * 20,
            .drop = -WALL_L * 20,
            .fly = STEP_L / 16,
            .block_mask = BOX_BLOCKED,
        };
    }

    ASSERT_FAIL();
    return (LOT_SETUP) {};
}

void LOT_InitialiseArray(void)
{
    m_BaddieSlots =
        GameBuf_Alloc(LOT_SLOT_COUNT * sizeof(CREATURE), GBUF_CREATURE_DATA);

    for (int32_t i = 0; i < LOT_SLOT_COUNT; i++) {
        CREATURE *const creature = &m_BaddieSlots[i];
        creature->item_num = NO_ITEM;
        creature->lot.node =
            GameBuf_Alloc(Box_GetCount() * sizeof(BOX_NODE), GBUF_CREATURE_LOT);
    }

    m_SlotsUsed = 0;
}

CREATURE *LOT_GetBaddieSlot(const int32_t i)
{
    return &m_BaddieSlots[i];
}

void LOT_DisableBaddieAI(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    CREATURE *const creature = item->creature_data;
    item->creature_data = nullptr;
    item->extra_rotations = nullptr;

    if (creature != nullptr) {
        creature->item_num = NO_ITEM;
        m_SlotsUsed--;
    }
}

bool LOT_EnableBaddieAI(const int16_t item_num, const bool always)
{
    if (Item_Get(item_num)->creature_data != nullptr) {
        return true;
    }

    if (m_SlotsUsed < LOT_SLOT_COUNT) {
        for (int32_t slot = 0; slot < LOT_SLOT_COUNT; slot++) {
            if (m_BaddieSlots[slot].item_num == NO_ITEM) {
                LOT_InitialiseSlot(item_num, slot);
                return true;
            }
        }
        ASSERT_FAIL();
    }

    int32_t worst_dist = 0;
    if (!always) {
        const ITEM *const item = Item_Get(item_num);
        const int32_t dx = (item->pos.x - g_Camera.pos.pos.x) >> 8;
        const int32_t dy = (item->pos.y - g_Camera.pos.pos.y) >> 8;
        const int32_t dz = (item->pos.z - g_Camera.pos.pos.z) >> 8;
        worst_dist = SQUARE(dx) + SQUARE(dy) + SQUARE(dz);
    }

    int32_t worst_slot = -1;
    for (int32_t slot = 0; slot < LOT_SLOT_COUNT; slot++) {
        const ITEM *const item = Item_Get(m_BaddieSlots[slot].item_num);
        const int32_t dx = (item->pos.x - g_Camera.pos.pos.x) >> 8;
        const int32_t dy = (item->pos.y - g_Camera.pos.pos.y) >> 8;
        const int32_t dz = (item->pos.z - g_Camera.pos.pos.z) >> 8;
        const int32_t dist = SQUARE(dx) + SQUARE(dy) + SQUARE(dz);
        if (dist > worst_dist) {
            worst_dist = dist;
            worst_slot = slot;
        }
    }

    if (worst_slot < 0) {
        return false;
    }

    const CREATURE *const creature = &m_BaddieSlots[worst_slot];
    Item_Get(creature->item_num)->status = IS_INVISIBLE;
    LOT_DisableBaddieAI(creature->item_num);
    LOT_InitialiseSlot(item_num, worst_slot);
    return true;
}

void LOT_InitialiseSlot(const int16_t item_num, const int32_t slot)
{
    CREATURE *const creature = &m_BaddieSlots[slot];
    ITEM *const item = Item_Get(item_num);
    item->creature_data = creature;
    item->extra_rotations = creature->joint_rotation;

    creature->item_num = item_num;
    creature->mood = MOOD_BORED;
    creature->neck_rotation = 0;
    creature->head_rotation = 0;
    creature->joint_rotation[0] = 0;
    creature->joint_rotation[1] = 0;
    creature->joint_rotation[2] = 0;
    creature->joint_rotation[3] = 0;
    creature->maximum_turn = DEG_1;
    creature->flags = 0;
    creature->enemy = nullptr;
    creature->head_left = false;
    creature->head_right = false;
    creature->reached_goal = false;
    creature->hurt_by_lara = false;
    creature->patrol_2 = false;
    creature->alerted = false;

    const OBJECT *const obj = Object_Get(item->object_id);
    creature->lot.setup = obj->lot_setup;

    LOT_ClearLOT(&creature->lot);
    LOT_CreateZone(item);

    m_SlotsUsed++;
}

void LOT_CreateZone(ITEM *const item)
{
    CREATURE *const creature = item->creature_data;

    const int16_t *zone;
    const int16_t *flip;
    if (creature->lot.setup.fly) {
        zone = Box_GetFlyZone(false);
        flip = Box_GetFlyZone(true);
    } else {
        zone = Box_GetGroundZone(false, BOX_ZONE(creature->lot.setup.step));
        flip = Box_GetGroundZone(true, BOX_ZONE(creature->lot.setup.step));
    }

    const ROOM *const room = Room_Get(item->room_num);
    item->box_num = Room_GetWorldSector(room, item->pos.x, item->pos.z)->box;

    int16_t zone_num = zone[item->box_num];
    int16_t flip_num = flip[item->box_num];

    const bool use_fixed_fly_zone =
        g_TRVersion == 3 && creature->lot.setup.fly != 0;

    creature->lot.zone_count = 0;
    BOX_NODE *node = creature->lot.node;
    for (int32_t i = 0; i < Box_GetCount(); i++) {
        if (use_fixed_fly_zone || zone[i] == zone_num || flip[i] == flip_num) {
            node->box_num = i;
            node++;
            creature->lot.zone_count++;
        }
    }
}

void LOT_InitialiseLOT(LOT_INFO *const lot)
{
    lot->node =
        GameBuf_Alloc(sizeof(BOX_NODE) * Box_GetCount(), GBUF_CREATURE_LOT);
    LOT_ClearLOT(lot);
}

void LOT_ClearLOT(LOT_INFO *const lot)
{
    lot->search_num = 0;
    lot->head = NO_BOX;
    lot->tail = NO_BOX;
    lot->target_box = NO_BOX;
    lot->required_box = NO_BOX;

    for (int32_t i = 0; i < Box_GetCount(); i++) {
        BOX_NODE *const node = &lot->node[i];
        node->next_expansion = NO_BOX;
        node->exit_box = NO_BOX;
        node->search_num = 0;
    }
}
