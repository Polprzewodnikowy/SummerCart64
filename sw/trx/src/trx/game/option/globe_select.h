#pragma once

#include <trx/game/inventory_ring/types.h>

void Option_GlobeSelect_Control(INVENTORY_ITEM *inv_item, bool is_busy);
void Option_GlobeSelect_Draw(INVENTORY_ITEM *inv_item);
void Option_GlobeSelect_Close(void);
void Option_GlobeSelect_Shutdown(void);

void Option_GlobeSelect_UpdateSelectable(INV_RING *ring);
int32_t Option_GlobeSelect_AreaFromMeshIdx(int32_t mesh_idx);
