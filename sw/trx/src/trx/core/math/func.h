#pragma once

#include <trx/core/math/types.h>

uint32_t Math_Sqrt(uint32_t n);
uint64_t Math_Sqrt64(uint64_t n);

void Math_GetVectorAngles(int32_t x, int32_t y, int32_t z, int16_t *dest);
int32_t Math_AngleInCone(int32_t angle1, int32_t angle2, int32_t cone);
DIRECTION Math_GetDirection(int16_t angle);
DIRECTION Math_GetDirectionCone(int16_t angle, int16_t cone);
int16_t Math_DirectionToAngle(DIRECTION dir);
int32_t Math_AngleMean(int32_t angle1, int32_t angle2, double ratio);
int32_t Math_FloorDiv(int32_t x, int32_t divisor);
int32_t Math_GCD(int32_t a, int32_t b);
