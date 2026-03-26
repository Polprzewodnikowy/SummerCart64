#pragma once

#include <trx/core/math.h>
#include <trx/game/game_flow/types.h>
#include <trx/game/stats/types.h>

bool Stats_HasSecret(int16_t secret_idx);
bool Stats_RemoveSecret(int16_t secret_idx);
bool Stats_AddSecret(int16_t secret_idx);
bool Stats_IsSecretValid(int16_t secret_idx);
OBJECT_ID Stats_GetSecretObject(int32_t secret_idx);
uint32_t Stats_GetSecretMaskForItem(const GF_LEVEL *level, int16_t item_num);

void Stats_UpdateSecrets(LEVEL_STATS *stats);
void Stats_MarkSecretCollected(const ITEM *item);
bool Stats_CheckAllSecretsCollected(void);
bool Stats_CheckAllLevelSecretsPickedUp(void);

void Stats_UpdateTimer(void);
void Stats_AddKill(void);
void Stats_AddPickup(void);
void Stats_AddAmmoHits(void);
void Stats_AddAmmoUsed(void);
void Stats_AddDeath(void);
void Stats_AddMedipacksUsed(double medipack_value);
void Stats_AddDistanceTravelled(XYZ_32 pos, XYZ_32 last_pos);
void Stats_MarkAlliesHostile(void);

FINAL_STATS Stats_ComputeFinalStats(bool include_bonus_levels);
