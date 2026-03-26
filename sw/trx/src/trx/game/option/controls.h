#pragma once

#include <trx/game/input.h>
#include <trx/game/inventory_ring/types.h>

void Option_Controls_Control(INVENTORY_ITEM *inv_item, bool is_busy);
void Option_Controls_Draw(INVENTORY_ITEM *inv_item);
void Option_Controls_Close(void);
void Option_Controls_Shutdown(void);
