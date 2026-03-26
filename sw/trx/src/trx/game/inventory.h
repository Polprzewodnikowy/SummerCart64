#pragma once

#include <trx/game/inventory_ring/types.h>
#include <trx/game/lara/types.h>
#include <trx/game/objects/ids.h>

#include <stdint.h>

extern INVENTORY_MODE g_Inv_Mode;

bool Inv_AddItemNTimes(OBJECT_ID obj_id, int32_t qty);
OBJECT_ID Inv_GetItemOption(OBJECT_ID obj_id);
OBJECT_ID Inv_GetItemPickup(OBJECT_ID obj_id);
void Inv_InsertItem(INVENTORY_ITEM *inv_item);
void Inv_InsertItemEx(INVENTORY_ITEM *inv_item, int32_t qty);
bool Inv_RemoveItem(OBJECT_ID obj_id);
int32_t Inv_RequestItem(OBJECT_ID obj_id);
void Inv_ClearSelection(void);
void Inv_RemoveAllItems(void);

bool Inv_AddItem(OBJECT_ID obj_id);
bool Inv_AddPickup(const ITEM *item);
