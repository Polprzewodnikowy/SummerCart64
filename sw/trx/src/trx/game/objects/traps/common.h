#pragma once

#include <trx/game/items.h>

typedef struct TRAP_DATA {
    XYZ_32 pos;
    int16_t room_num;
} TRAP_DATA;

void Trap_Initialise(int16_t item_num);
void Trap_Reset(ITEM *item);
