#pragma once

#include <trx/game/items/types.h>

void Lara_Electricity_UpdatePoints(void);
void Lara_Electricity_EmitLight(void);
void Lara_Electricity_Draw(int32_t lr, const ITEM *item);
XYZ_16 Lara_Electricity_GetPoint(int32_t idx);
