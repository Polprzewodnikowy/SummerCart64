#pragma once

#include <trx/game/rooms/types.h>

BOUNDS_32 Room_GetRoomBounds(const ROOM *room);
SECTOR *Room_GetSector(XYZ_32 pos, int16_t *room_num);
SECTOR *Room_GetSectorOnWalkable(XYZ_32 pos, int16_t *room_num);
SECTOR *Room_GetWorldSector(const ROOM *room, int32_t x_pos, int32_t z_pos);
SECTOR *Room_GetUnitSector(
    const ROOM *room, int32_t x_sector, int32_t z_sector);
SECTOR *Room_GetPitSector(const SECTOR *sector, int32_t x, int32_t z);
SECTOR *Room_GetSkySector(const SECTOR *sector, int32_t x, int32_t z);

void Room_SetAbyssHeight(int16_t height);
bool Room_IsAbyssHeight(int32_t height);

HEIGHT_TYPE Room_GetHeightType(void);
int16_t Room_GetTiltType(const SECTOR *sector, XYZ_32 pos);

int32_t Room_GetHeight(const SECTOR *sector, XYZ_32 pos);
int32_t Room_GetHeightEx(
    const SECTOR *sector, XYZ_32 pos, bool fix_tilts, int16_t ignore_item_num);
int32_t Room_GetCeiling(const SECTOR *sector, XYZ_32 pos);
int32_t Room_GetCeilingEx(const SECTOR *sector, XYZ_32 pos, bool fix_tilts);
int32_t Room_GetFloorHeightForSector(
    const SECTOR *sector, int32_t x, int32_t z, bool fix_tilts);

int32_t Room_GetWaterHeight(XYZ_32 pos, int16_t room_num);
int32_t Room_GetWaterHeightEx(XYZ_32 pos, int16_t room_num, bool fix_tilts);
void Room_AlterFloorHeight(const ITEM *item, int32_t height);

int32_t Room_FindGridShift(int32_t src, int32_t dst);

bool Room_IsOnWalkable(
    const SECTOR *sector, XYZ_32 pos, int32_t room_height,
    int16_t ignore_item_num);
