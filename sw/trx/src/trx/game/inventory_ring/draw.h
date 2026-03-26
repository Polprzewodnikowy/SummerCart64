#pragma once

#include <trx/game/inventory_ring/types.h>

void InvRing_Draw(INV_RING *ring);

const INVENTORY_ITEM *InvRing_GetInvItem(OBJECT_ID obj_id);
