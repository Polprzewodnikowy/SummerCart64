#pragma once

#include <trx/game/rooms/types.h>

void OutputSource_RoomsDebug_Init(void);
void OutputSource_RoomsDebug_Shutdown(void);
void OutputSource_RoomsDebug_ObserveLevelLoad(void);
void OutputSource_RoomsDebug_ObserveLevelUnload(void);
void OutputSource_RoomsDebug_ObserveRoomFlip(const ROOM *room);

void OutputSource_RoomsDebug_StageRoom(const ROOM *room);
