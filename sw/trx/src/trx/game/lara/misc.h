#pragma once

#include <trx/game/collision.h>
#include <trx/game/game_flow/enum.h>
#include <trx/game/lara/enum.h>

void Lara_GetJointAbsPosition(XYZ_32 *vec, LARA_MESH joint);
void Lara_RefuseInteraction(void);
void Lara_TakeHit(ITEM *lara_item, int32_t dx, int32_t dz);
void Lara_Extinguish(void);
void Lara_TouchLava(void);
void Lara_TouchDeathSector(GF_DEATH_TILE death_tile);
void Lara_RapidsDrown(void);

int32_t Lara_FloorFront(const ITEM *item, int16_t ang, int32_t dist);
int32_t Lara_CeilingFront(
    const ITEM *item, int16_t ang, int32_t dist, int32_t item_height);
void Lara_CatchFire(void);

void Lara_UpdateRoomToHeight(int32_t height);
int32_t Lara_GetWaterDepth(int32_t x, int32_t y, int32_t z, int16_t room_num);

// Returns true if Lara has the M16 equipped and is in either anim state: 0
// (start aim); 2 (firing); or 4 (stopping firing).
bool Lara_IsM16Active(void);
bool Lara_HasState(const LARA_TRX_STATE *test_arr);
bool Lara_HasExtraState(const LARA_EXTRA_STATE *test_arr);
void Lara_SwitchToExtraState(LARA_EXTRA_STATE goal_state);
