#pragma once

#include <trx/game/sparks/types.h>

void Sparks_Reset(void);
void Sparks_Control(void);
void Sparks_Draw(void);

void Sparks_DetachEffect(int16_t effect_num);
void Sparks_DetachItem(int16_t item_num);

XYZ_32 Sparks_GetWorldPos(const SPARK *spark);

SPARK *Sparks_GetFreeSpark(void);
SPARK *Sparks_GetSpark(int32_t idx);
void Sparks_Sync(SPARK *spark);
void Sparks_FinishSetup(SPARK *spark);

int8_t Sparks_AllocDynamic(uint8_t flags);
void Sparks_FreeDynamic(int8_t idx);

XZ_32 Sparks_GetSmokeWind(void);
void Sparks_SetSmokeWind(XZ_32 wind);
int32_t Sparks_GetHairWindZ(void);
