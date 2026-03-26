#pragma once

#include <trx/game/output/mesh_batcher/batcher.h>
#include <trx/game/rooms/types.h>

void OutputSource_Rooms_Init(MESH_BATCHER *batcher);
void OutputSource_Rooms_Shutdown(void);
void OutputSource_Rooms_ObserveLevelLoad(void);
void OutputSource_Rooms_ObserveLevelUnload(void);
void OutputSource_Rooms_ObserveRoomFlip(const ROOM *room);

void OutputSource_Rooms_StageRoom(const ROOM *room);
