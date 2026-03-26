#pragma once

#include <trx/game/inventory_ring/types.h>

void Option_Control(INVENTORY_ITEM *inv_item, bool is_busy);
void Option_Draw(INVENTORY_ITEM *inv_item);
void Option_Close(const INVENTORY_ITEM *inv_item);

// Reset internal positioning of option UIs.
void Option_Reset(void);

// Free up resources associated with option UIs.
void Option_Shutdown(void);
