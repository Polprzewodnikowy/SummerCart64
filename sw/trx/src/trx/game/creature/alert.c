#include <trx/core/utils.h>
#include <trx/game/creature.h>
#include <trx/game/items.h>
#include <trx/game/pathing.h>

void Creature_AlertNearbyGuards(const ITEM *const item)
{
    for (int32_t i = 0; i < LOT_SLOT_COUNT; i++) {
        CREATURE *const creature = LOT_GetBaddieSlot(i);
        if (creature->item_num == NO_ITEM) {
            continue;
        }

        const ITEM *const target = Item_Get(creature->item_num);
        if (target->room_num == item->room_num) {
            creature->alerted = true;
            continue;
        }

        int32_t dx = (target->pos.x - item->pos.x) >> 6;
        int32_t dy = (target->pos.y - item->pos.y) >> 6;
        int32_t dz = (target->pos.z - item->pos.z) >> 6;
        int32_t dist = SQUARE(dx) + SQUARE(dy) + SQUARE(dz);

        if (dist < 8000) {
            creature->alerted = true;
        }
    }
}

void Creature_AlertAllGuards(const int16_t item_num)
{
    const ITEM *const item = Item_Get(item_num);

    for (int32_t i = 0; i < LOT_SLOT_COUNT; i++) {
        CREATURE *const creature = LOT_GetBaddieSlot(i);
        if (creature->item_num == NO_ITEM) {
            continue;
        }

        const ITEM *const target = Item_Get(creature->item_num);
        if (target->object_id == item->object_id
            && target->status == IS_ACTIVE) {
            creature->alerted = true;
        }
    }
}
