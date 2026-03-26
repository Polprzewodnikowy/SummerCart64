#pragma once

#include <trx/game/game_flow/types.h>
#include <trx/game/inventory_ring/types.h>
#include <trx/game/objects/types.h>

typedef void (*INV_RING_BUTTON_HINT_DRAWER)(void *user_data);

INV_RING *InvRing_Open(INVENTORY_MODE mode);
void InvRing_Close(INV_RING *ring);

GF_COMMAND InvRing_Control(INV_RING *ring);
bool InvRing_IsRingAvailable(RING_TYPE ring_type);
INV_RING *InvRing_GetActiveRing(void);

void InvRing_AdjustMusicVolume(const INV_RING *ring);
void InvRing_SetRequestedObjectID(OBJECT_ID obj_id);
void InvRing_SetButtonHintDrawer(
    INV_RING_BUTTON_HINT_DRAWER draw_func, void *user_data);
void InvRing_ClearButtonHint(void);

void InvRing_RemoveAllText(void);

INVENTORY_ITEM *InvRing_GetByObjectID(OBJECT_ID object_id);
