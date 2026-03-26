#pragma once

#include <trx/game/items/types.h>
#include <trx/game/rooms/types.h>

typedef struct {
    bool drawn;
    bool shadow_drawn;
} OUTPUT_ITEM_BIND;

typedef struct {
    bool active;
    bool drawn;
    int16_t bound_left;
    int16_t bound_right;
    int16_t bound_top;
    int16_t bound_bottom;
    int16_t test_left;
    int16_t test_right;
    int16_t test_top;
    int16_t test_bottom;
} OUTPUT_ROOM_BIND;

void Output_Bind_ResetItems(void);
OUTPUT_ITEM_BIND *Output_Bind_GetItem(const ITEM *item);

void Output_Bind_ResetRooms(void);
OUTPUT_ROOM_BIND *Output_Bind_GetRoom(const ROOM *room);
