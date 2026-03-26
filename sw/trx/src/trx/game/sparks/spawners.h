#pragma once

#include <trx/game/items/types.h>
#include <trx/game/lara/enum.h>
#include <trx/game/types.h>

void Sparks_TriggerBubble(
    int32_t x, int32_t y, int32_t z, int32_t size, int32_t size_range,
    int16_t effect_num);

void Sparks_TriggerWaterfallMist(int32_t x, int32_t y, int32_t z, int32_t ang);

void Sparks_TriggerBreath(XYZ_32 pos, XYZ_32 vel, int16_t room_num);

void Sparks_TriggerUnderwaterExplosion(const ITEM *item);
void Sparks_TriggerExplosionBubble(XYZ_32 pos, int16_t room_num);
void Sparks_TriggerExplosionSparks(
    XYZ_32 pos, int32_t extras, int32_t dynamic, int32_t uw, int16_t room_num);
void Sparks_TriggerExplosionSmoke(XYZ_32 pos, bool uw, int16_t room_num);
void Sparks_TriggerExplosionSmokeEnd(XYZ_32 pos, bool uw, int16_t room_num);

void Sparks_TriggerFireFlame(XYZ_32 pos, int32_t body_part, int32_t type);
void Sparks_TriggerStaticFlame(XYZ_32 pos, int32_t size);
void Sparks_TriggerFireSmoke(XYZ_32 pos, int32_t body_part, int32_t type);
void Sparks_TriggerSideFlame(
    XYZ_32 pos, int32_t angle, int32_t speed, bool pilot);

void Sparks_TriggerDartSmoke(XYZ_32 pos, XZ_32 vel, bool hit);

void Sparks_TriggerFlareSparks(XYZ_32 pos, XYZ_32 vel, bool smoke);

void Sparks_TriggerRicochet(GAME_VECTOR pos, int32_t angle, int32_t size);

void Sparks_TriggerGunSmoke(
    GAME_VECTOR pos, bool initial, LARA_GUN_TYPE weapon, int32_t shade);
void Sparks_TriggerGunSmokeDirected(
    GAME_VECTOR pos, XYZ_32 vel, bool initial, LARA_GUN_TYPE weapon,
    int32_t shade);

void Sparks_TriggerShotgunSparks(XYZ_32 pos, XYZ_32 vel);

void Sparks_TriggerRocketSmoke(XYZ_32 pos, int32_t c, int16_t room_num);
void Sparks_TriggerRocketFlame(
    XYZ_32 pos, XYZ_32 vel, int16_t item_num, int16_t room_num);

void Sparks_TriggerBlood(XYZ_32 pos, int32_t angle_12, int32_t count);
void Sparks_TriggerBloodD(XYZ_32 pos, int32_t angle_12, int32_t count);

void Sparks_TriggerFlamethrowerHitFlame(XYZ_32 pos);
void Sparks_TriggerFlamethrowerSmoke(XYZ_32 pos, bool uw);
