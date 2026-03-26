#pragma once

#include <trx/config/types.h>
#include <trx/game/camera/types.h>

void Camera_Update(void);
void Camera_MoveManual(void);
void Camera_Apply(void);
bool Camera_IsChunky(void);
void Camera_SetChunky(bool is_chunky);
void Camera_Initialise(void);
void Camera_ResetPosition(void);
void Camera_Reset(void);
void Camera_ApplyBounce(void);
void Camera_ClampInterpResult(void);
const CAMERA_LOOK_SETTINGS *Camera_GetLookSettings(bool on_surface);

bool Camera_LOSCheck(GAME_VECTOR *start, GAME_VECTOR *target, int32_t shift);
bool Camera_Collide(GAME_VECTOR *ideal, int32_t shift, bool y_first);

void Camera_RegisterStrategy(CAMERA_MODE mode, CAMERA_STRATEGY strategy);

#define REGISTER_CAMERA(mode, strategy)                                        \
    __attribute__((constructor)) static void M_RegisterCamera##mode(void)      \
    {                                                                          \
        Camera_RegisterStrategy(mode, strategy);                               \
    }
