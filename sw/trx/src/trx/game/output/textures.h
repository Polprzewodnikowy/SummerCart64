#pragma once

#include <trx/game/output/scene_source.h>
#include <trx/game/output/types.h>

#include <GL/glew.h>

#pragma pack(push, 1)
typedef struct {
    float x0;
    float y0;
    float x1;
    float y1;
} OUTPUT_TEXTURE_SIZE;

typedef struct {
    float u;
    float v;
    float w;
} OUTPUT_UVW;
#pragma pack(pop)

void Output_Textures_Init(void);
void Output_Textures_Shutdown(void);
void Output_Textures_ObserveLevelLoad(void);
void Output_Textures_UpdateEnvironmentMap(void);
void Output_Textures_CycleAnimations(void);
void Output_Textures_ApplyRenderSettings(void);
GLuint Output_Textures_GetAtlasTexture(void);
GLuint Output_Textures_GetEnvMapTexture(void);

int32_t Output_Textures_GetObjectUVWIndex(
    int32_t texture_idx, int32_t face_idx);
int32_t Output_Textures_GetSpriteUVWIndex(
    int32_t texture_idx, int32_t face_idx);

OUTPUT_UVW Output_Textures_GetUVW(int32_t uvw_idx);
OUTPUT_TEXTURE_SIZE Output_Textures_GetAtlasSize(int32_t uvw_idx);
bool Output_Textures_IsObjectTextureAnimated(int32_t texture_idx);
SCENE_PASS Output_Textures_GetObjectTextureScenePass(int32_t texture_idx);
bool Output_Textures_IsSpriteTextureAnimated(int32_t sprite_idx);
uint16_t Output_Textures_GetSpriteTextureFlags(int32_t sprite_idx);
void Output_Textures_SetSpriteTextureFlags(int32_t sprite_idx, uint16_t flags);

// Public utility methods =====================================================

void Output_InitialiseTexturePages(int32_t num_pages, bool use_8bit);
void Output_InitialisePalettes(
    int32_t palette_size, const RGB_888 *palette_8, const RGB_888 *palette_16);
void Output_InitialiseObjectTextures(int32_t num_textures);
void Output_InitialiseSpriteTextures(int32_t num_textures);
void Output_InitialiseAnimatedTextures(int32_t num_ranges);

int32_t Output_GetTexturePageCount(void);
uint8_t *Output_GetTexturePage8(int32_t page_idx);
RGBA_8888 *Output_GetTexturePage32(int32_t page_idx);

void Output_LockTexturePage32(int32_t page_idx);
void Output_UnlockTexturePage32(int32_t page_idx);

int32_t Output_GetPaletteSize(void);
RGB_888 Output_GetPaletteColor8(uint16_t idx);
RGB_888 Output_GetPaletteColor16(uint16_t idx);
LIGHT_MAP *Output_GetLightMap(uint8_t idx);
SHADE_MAP *Output_GetShadeMap(uint8_t idx);
int32_t Output_GetObjectTextureCount(void);
int32_t Output_GetSpriteTextureCount(void);
OBJECT_TEXTURE *Output_GetObjectTexture(int32_t texture_idx);
SPRITE_TEXTURE *Output_GetSpriteTexture(int32_t texture_idx);
ANIMATED_TEXTURE_RANGE *Output_GetAnimatedTextureRange(int32_t range_idx);

RGBA_8888 Output_RGB2RGBA(RGB_888 color);
RGBA_F Output_RGB2RGBA_F(RGB_F color);
int16_t Output_FindColor8(RGB_888 color);
void Output_CycleAnimatedTextures(void);
