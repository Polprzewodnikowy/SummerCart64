#pragma once

#include <trx/game/effects/types.h>

void Effect_InitialiseArray(void);
void Effect_Control(void);

EFFECT *Effect_Get(int16_t effect_num);
int16_t Effect_GetIndex(const EFFECT *effect);
int16_t Effect_GetInOrderNum(int16_t effect_num);
int16_t Effect_GetActiveNum(void);
int16_t Effect_Create(int16_t room_num);
void Effect_Kill(int16_t effect_num);
void Effect_KillAllActive(void);
void Effect_UpdateRoom(int16_t effect_num, int16_t room_num);
