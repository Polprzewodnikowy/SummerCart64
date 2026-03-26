#pragma once

#include <stdint.h>

void Random_Seed(void);
void Random_SeedControl(int32_t seed);
void Random_SeedDraw(int32_t seed);

int32_t Random_GetControl(void);
int32_t Random_GetDraw(void);
int32_t Random_GetControlSeed(void);
int32_t Random_GetDrawSeed(void);

void Random_FreezeDraw(bool is_frozen);
