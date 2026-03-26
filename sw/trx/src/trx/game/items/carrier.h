#pragma once

#include <trx/game/game_flow/types.h>
#include <trx/game/items/types.h>

void Carrier_InitialiseLevel(const GF_LEVEL *level);
int32_t Carrier_GetItemCount(int16_t item_num);
bool Carrier_IsItemCarried(int16_t item_num);
void Carrier_TestItemDrops(int16_t item_num);

void Carrier_SyncItem(int16_t item_num, CARRIED_ITEM *carried_item);
DROP_STATUS Carrier_GetSaveStatus(const CARRIED_ITEM *item);

void Carrier_AnimateDrops(void);
