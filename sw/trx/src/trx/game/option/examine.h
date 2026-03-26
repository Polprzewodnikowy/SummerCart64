#pragma once

#include <trx/game/inventory_ring/types.h>

bool Option_Examine_CanExamine(OBJECT_ID obj_id);
void Option_Examine_Control(INVENTORY_ITEM *inv_item, bool is_busy);
void Option_Examine_Draw(void);
void Option_Examine_Close(void);
