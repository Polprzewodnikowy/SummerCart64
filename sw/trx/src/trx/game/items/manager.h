#pragma once

#include <trx/game/anims.h>
#include <trx/game/items/types.h>

void Item_InitialiseItems(int32_t num_items);
ITEM *Item_Get(int16_t num);
int16_t Item_GetIndex(const ITEM *item);
ITEM *Item_Find(OBJECT_ID obj_id);

int32_t Item_GetLevelCount(void);
int32_t Item_GetTotalCount(void);

int16_t Item_GetNextActive(void);
int16_t Item_GetPrevActive(void);
void Item_SetPrevActive(int16_t item_num);

int16_t Item_Create(void);
int16_t Item_CreateLevelItem(void);
int16_t Item_Spawn(const ITEM *item, OBJECT_ID obj_id);

void Item_Initialise(int16_t item_num);
void Item_Control(void);
void Item_Kill(int16_t item_num);
void Item_KillAllActive(void);
void Item_RemoveActive(int16_t item_num);
void Item_RemoveDrawn(int16_t item_num);
void Item_ClearKilled(void);
void Item_AddActive(int16_t item_num);
void Item_UpdateRoom(int16_t item_num, int16_t room_num);

int32_t Item_GlobalReplace(OBJECT_ID src_obj_id, OBJECT_ID dst_obj_id);

// Set the name of the item, storing a copy of the provided string.
// Returns false if the name is already used by another item.
bool Item_SetName(int16_t item_num, const char *name);

// Retrieve an item by its name, or nullptr if not found
ITEM *Item_GetByName(const char *name);
