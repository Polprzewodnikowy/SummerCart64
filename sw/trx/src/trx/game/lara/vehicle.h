#pragma once

#include <trx/game/items/types.h>

bool Lara_Vehicle_IsMounted(void);
bool Lara_Vehicle_IsOnType(OBJECT_ID obj_id);
void Lara_Vehicle_SetIndex(int16_t item_num);
int16_t Lara_Vehicle_GetIndex(void);
ITEM *Lara_Vehicle_GetItem(void);

void Lara_Vehicle_Dismount(void);
