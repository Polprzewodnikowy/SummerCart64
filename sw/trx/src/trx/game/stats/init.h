#pragma once

#include <trx/game/game_flow/types.h>
#include <trx/game/stats/types.h>

LEVEL_MAX_STATS *Stats_GetLevelMaxStats(const GF_LEVEL *level);
void Stats_CalculateMaxStats(void);
