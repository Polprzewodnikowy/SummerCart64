#pragma once

#include <trx/game/inventory_ring/types.h>
#include <trx/game/phase/types.h>

PHASE *Phase_Inventory_Create(INVENTORY_MODE mode);
void Phase_Inventory_Destroy(PHASE *phase);
