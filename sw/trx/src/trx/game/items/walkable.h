#pragma once

#include <trx/core/math.h>
#include <trx/game/types.h>

#include <stdint.h>

void Walkable_AllocateNodes(const ITEM *item, int32_t footprint);
void Walkable_Add(int16_t item_num, XYZ_32 pos);
void Walkable_Remove(int16_t item_num);
void Walkable_Reset(void);
void Walkable_ResetLevel(void);
void Walkable_Shutdown(void);
void Walkable_Reposition(
    int16_t item_num, GAME_VECTOR start, GAME_VECTOR target);
