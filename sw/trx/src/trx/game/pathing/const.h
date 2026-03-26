#pragma once

#include <trx/game/const.h>
#include <trx/version.h>

#define NO_BOX (-1)
#define BOX_ZONE(num) (((num) / STEP_L) - 1)
#define LOT_SLOT_COUNT 32
#define MAX_ZONES 4

#define BOX_BLOCKED 0x4000
#define BOX_BLOCKED_SEARCH 0x8000
#define BOX_BLOCKABLE 0x8000
#define BOX_FIXED_FLY_ZONE 0x2000
