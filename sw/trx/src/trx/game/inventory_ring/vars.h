#pragma once

#include <trx/core/vector.h>
#include <trx/game/camera/types.h>
#include <trx/game/inventory_ring/types.h>

extern CAMERA_INFO g_InvRing_OldCamera;
extern INV_RING_SOURCE g_InvRing_Source[RT_NUMBER_OF];
extern VECTOR *g_InvRing_Items;

void InvRing_LoadVars(const char *path);
