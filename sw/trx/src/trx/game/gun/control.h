#pragma once

#include <trx/game/items/types.h>
#include <trx/game/lara/enum.h>

int32_t Gun_FireWeapon(
    LARA_GUN_TYPE weapon_type, ITEM *target, const ITEM *src,
    const int16_t *angles);

void Gun_Control(void);
void Gun_EnsureReady(void);
