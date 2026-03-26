#pragma once

#include <trx/game/camera/types.h>
#include <trx/game/game_flow/types.h>

bool Cutscene_Start(int32_t level_num);
void Cutscene_End(void);
GF_COMMAND Cutscene_Control(void);
void Cutscene_Draw(void);

CAMERA_INFO *Cutscene_GetCamera(void);
