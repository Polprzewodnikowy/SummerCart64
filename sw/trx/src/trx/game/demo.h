#pragma once

#include <trx/core/virtual_file.h>
#include <trx/game/game_flow/types.h>

void Demo_LoadData(VFILE *file, size_t size);
uint32_t *Demo_GetData(void);

bool Demo_Start(int32_t level_num);
void Demo_End(void);
void Demo_Pause(void);
void Demo_Unpause(void);
void Demo_StopFlashing(void);

bool Demo_UpdateInput(void);
GF_COMMAND Demo_Control(void);
int32_t Demo_ChooseLevel(int32_t demo_num);
