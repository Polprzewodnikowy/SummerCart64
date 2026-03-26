#pragma once

#include <trx/core/colors.h>
#include <trx/core/vector.h>
#include <trx/game/objects/types.h>
#include <trx/game/rooms.h>
#include <trx/game/viewport.h>

void Output_Lights_Init(void);
void Output_Lights_Shutdown(void);
void Output_Lights_ObserveLevelLoad(void);

void Output_CalculateLight(XYZ_32 pos, int16_t room_num);
void Output_CalculateStaticLight(int16_t adder);
void Output_CalculateStaticLightRGB15(int16_t rgb15);
void Output_CalculateStaticLightRGB_F(RGB_F rgb);
void Output_CalculateStaticMeshLight(XYZ_32 pos, SHADE shade, const ROOM *room);
void Output_CalculateObjectLighting(const ITEM *item, const BOUNDS_16 *bounds);
int32_t Output_GetRoomLightShade(ROOM_LIGHT_MODE mode);

int32_t Output_GetSunsetDuration(void);
void Output_SetSunsetEnabled(bool enabled);
int16_t Output_GetSkyShade(void);

void Output_ResetDynamicLights(void);
void Output_AddDynamicLight(XYZ_32 pos, int32_t intensity, int32_t falloff);
void Output_AddDynamicLightRGB(XYZ_32 pos, int32_t falloff, RGB_888 color);
VECTOR *Output_GetDynamicLights(void);

void Output_AnimateLights(int32_t num_frames);
