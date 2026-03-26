#pragma once

#include <trx/core/utils.h>
#include <trx/game/const.h>

#define FRONT_ARC DEG_90
#define UNIT_SHADOW 256

#define CREATURE_STALK_DIST (3 * WALL_L) // = 3072
#define CREATURE_ESCAPE_DIST (5 * WALL_L) // = 5120
#define CREATURE_TARGET_DIST (4 * WALL_L) // = 4096

#define CREATURE_MISS_CHANCE 0x2000

#define CREATURE_SHOOT_RANGE SQUARE((g_TRVersion == 1 ? 7 : 8) * WALL_L)
// = 51380224 (TR1), 67108864 (TR2)
