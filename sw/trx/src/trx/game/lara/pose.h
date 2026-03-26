#pragma once

#include <trx/game/lara/enum.h>
#include <trx/game/types.h>

typedef struct {
    XYZ_16 offset;
    XYZ_16 rots[LM_NUMBER_OF];
} LARA_POSE;

void Lara_Pose_Init();
void Lara_Pose_Shutdown();

bool Lara_Pose_IsAvailable(void);
void Lara_Pose_Clear(void);
void Lara_Pose_Cycle(int32_t dir);
const LARA_POSE *Lara_Pose_Get(void);
