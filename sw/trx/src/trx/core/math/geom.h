#pragma once

#include <trx/core/math/types.h>

XYZ_32 XYZ_32_From16(XYZ_16 src);
int16_t XYZ_32_GetYaw(XYZ_32 pos);
int16_t XYZ_32_GetYawDiff(XYZ_32 pos1, const XYZ_32 pos2);
int16_t XYZ_32_GetPitch(XYZ_32 pos);
int32_t XYZ_32_GetDistance(XYZ_32 pos1, XYZ_32 pos2);

// Take length of a vector
int32_t XYZ_32_GetLength(XYZ_32 pos);

// Take squared length of a vector
int32_t XYZ_32_GetLength2(XYZ_32 pos);
int64_t XYZ_32_GetLength2_64(XYZ_32 pos);

int64_t XYZ_32_DotProduct_64(XYZ_32 a, XYZ_32 b);

bool XYZ_32_AreEquivalent(XYZ_32 pos1, XYZ_32 pos2);
bool XYZ_32_IsNearby(XYZ_32 pos1, XYZ_32 pos2, int32_t distance);

XYZ_32 XYZ_32_FromYawPitch(int16_t yaw, int16_t pitch, int32_t distance);
XYZ_32 XYZ_32_OffsetYaw(XYZ_32 src, int16_t yaw, int32_t distance);

bool XYZ_32_ProjectPointOntoAxis(
    XYZ_32 origin, XYZ_32 axis, int64_t axis_len2, XYZ_32 *pos);

bool XYZ_16_AreEquivalent(XYZ_16 rot1, XYZ_16 rot2);

float XYZ_F_DotProduct(XYZ_F a, XYZ_F b);
float XYZ_F_Length2(XYZ_F pos);
float XYZ_F_Length(XYZ_F pos);
XYZ_F XYZ_F_Subtract(XYZ_F a, XYZ_F b);
