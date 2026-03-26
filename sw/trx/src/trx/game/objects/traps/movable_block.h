#pragma once

#include <trx/game/objects/types.h>
#include <trx/game/rooms.h>

// Block or unblock a block's box overlap index.
void MovableBlock_UpdateBox(const ITEM *item, bool blocked);

// Drop a stack of blocks.
void MovableBlock_DropStack(XYZ_32 drop_pos, int16_t room_num);

// Shift a stack of blocks up or down in the y direction.
void MovableBlock_ShiftStackY(
    int32_t stack_height, XYZ_32 old_pos, int32_t new_y, int16_t room_num,
    bool reposition);

// Shift a stack of blocks in the x or z direction..
void MovableBlock_SlideStack(
    int32_t stack_height, XYZ_32 old_sector, const ITEM *dest_item,
    bool reposition);
