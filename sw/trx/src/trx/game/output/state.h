#pragma once

#include <trx/core/colors.h>
#include <trx/core/math/types.h>
#include <trx/game/output/uniforms.h>
#include <trx/game/rooms.h>

#include <GL/glew.h>

void Output_SetSkyboxEnabled(bool enabled);
bool Output_IsSkyboxEnabled(void);

void Output_GetPerspProjectionMatrix(GLfloat output[][4]);
void Output_GetOrthoProjectionMatrix(GLfloat output[][4]);

void Output_SetTime(float time);
float Output_GetTime(void);
float Output_GetTimeInGame(void);
void Output_SetTimeInGame(float time);
bool Output_IsControlFrame(void);
void Output_SetControlFrame(bool is_control_frame);

void Output_SetupBelowWater(bool is_underwater);
void Output_SetupAboveWater(bool is_underwater);
RGB_F Output_GetWaterColor(void);
void Output_SetWaterColor(RGB_888 color);

RGB_F Output_GetTint(void);
void Output_PushTintOverride(RGB_F tint);
void Output_PopTintOverride(void);
bool Output_GetWaterEffect(void);
bool Output_GetWibbleEffect(void);
float Output_GetDesaturation(void);
void Output_SetDesaturation(float desaturation);
RGB_F Output_GetGlobalTint(void);
void Output_SetGlobalTint(RGB_F tint);

RGBA_F Output_GetFogColor(void);
int32_t Output_GetFogStart(void);
int32_t Output_GetFogEnd(void);

void Output_SetFogColor(RGBA_8888 color);
void Output_SetFogStart(int32_t dist);
void Output_SetFogEnd(int32_t dist);

int32_t Output_GetNearZ(void);
int32_t Output_GetFarZ(void);
int32_t Output_GetNearZ_UI(void);
int32_t Output_GetFarZ_UI(void);

void Output_SetCurrentRoom(const ROOM *room_num);
const ROOM *Output_GetCurrentRoom(void);

int32_t Output_GetLightAdder(void);
int32_t Output_GetLightDivider(void);
XYZ_32 Output_GetLightVectorView(void);
OUTPUT_LIGHT_INFO Output_GetLightInfo(void);
void Output_SetLightAdder(int32_t adder);
void Output_SetLightDivider(int32_t divider);
void Output_RotateLight(int16_t pitch, int16_t yaw);
void Output_SetTR3Light(
    RGB_F ambient, const RGB_F colors[3], const XYZ_32 dirs_view[3]);

void Output_EnableScissor(float x, float y, float w, float h);
void Output_DisableScissor(void);

void Output_AdjustDepth(float factor, float units);

void Output_SetupBelowWater(bool underwater);
void Output_SetupAboveWater(bool underwater);

void Output_AnimateTextures(int32_t num_frames);
