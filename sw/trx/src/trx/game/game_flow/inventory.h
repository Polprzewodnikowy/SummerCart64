#pragma once

#include <trx/game/game_flow/types.h>

void GF_InventoryModifier_Scan(const GF_LEVEL *level);
void GF_InventoryModifier_Apply(const GF_LEVEL *level, GF_INV_TYPE type);
void GF_InventoryModifier_ApplyToResumeInfo(const GF_LEVEL *level);
int32_t GF_GetSecretRewardCount(const GF_LEVEL *level);
