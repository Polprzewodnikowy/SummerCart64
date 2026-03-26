#pragma once

#include <trx/core/math/types.h>
#include <trx/game/lara/types.h>

bool Lara_Cheat_GiveAllKeys(void);
bool Lara_Cheat_GiveAllGuns(bool ignore_exclusions);
bool Lara_Cheat_GiveGun(LARA_GUN_TYPE gun_type, bool ignore_exclusions);
bool Lara_Cheat_GiveAllItems(void);
void Lara_Cheat_GetStuff(void);
void Lara_Cheat_EndLevel(void);
bool Lara_Cheat_KillEnemy(int16_t item_num);
bool Lara_Cheat_OpenNearestDoor(void);
bool Lara_Cheat_EnterFlyMode(void);
bool Lara_Cheat_ExitFlyMode(void);
bool Lara_Cheat_Teleport(XYZ_32 pos, int16_t room_num);
