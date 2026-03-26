#pragma once

#include <trx/game/const.h>

#define ROUND_TO_CLICK(V) ((V) & ~(STEP_L - 1))
#define ROUND_TO_SECTOR(V) ((V) & ~(WALL_L - 1))
#define ROUND_TO_SECTOR_END(V) ((V) | (WALL_L - 1))
#define ROUND_TO_CLICK_UP(V) (((V) + STEP_L / 2 - 1) & ~(STEP_L / 2 - 1))
#define ROUND_TO_HALF_CLICK(V) ((V) & ~(STEP_L / 2 - 1))
#define ROUND_TO_CLICK_SIGNED(V)                                               \
    (((V) >= 0) ? (((V) + STEP_L - 1) & ~(STEP_L - 1)) : ((V) & ~(STEP_L - 1)))
