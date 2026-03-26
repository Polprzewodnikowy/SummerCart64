#pragma once

#include <trx/game/inventory_ring/types.h>

void Option_Stats_Control(INVENTORY_ITEM *inv_item, bool is_busy);
void Option_Stats_Draw(void);
void Option_Stats_Close(void);

void Option_Stats_UpdateCompassNeedle(const INVENTORY_ITEM *inv_item);
int16_t Option_Stats_GetCompassNeedleAngle(void);
