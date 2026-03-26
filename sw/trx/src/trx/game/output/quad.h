#pragma once

#include <GL/glew.h>
#include <stdint.h>

// Textured screen-space quad renderer used by output code paths such as FMV
// presentation and overlay composition. It owns GL state/resources needed to
// upload 2D image data or bind external textures, then draw them with optional
// fit/repeat/effect controls.

typedef struct {
    float u;
    float v;
} OUTPUT_QUAD_SURFACE_UV;

typedef struct {
    int32_t width;
    int32_t height;
    int32_t bit_count;
    GLenum tex_format;
    GLenum tex_type;
    OUTPUT_QUAD_SURFACE_UV uv[4];
    int32_t pitch;
} OUTPUT_QUAD_SURFACE_DESC;

typedef struct {
    float x0;
    float y0;
    float x1;
    float y1;
} OUTPUT_QUAD_TEXTURE_SIZE;

typedef enum {
    OUTPUT_QUAD_EFFECT_NONE = 0,
    OUTPUT_QUAD_EFFECT_VIGNETTE = 1 << 0,
    OUTPUT_QUAD_EFFECT_WAVE = 1 << 1,
} OUTPUT_QUAD_EFFECT;

typedef struct OUTPUT_QUAD OUTPUT_QUAD;

typedef enum {
    OUTPUT_QUAD_FIT_STRETCH,
    OUTPUT_QUAD_FIT_LETTERBOX,
    OUTPUT_QUAD_FIT_CROP,
    OUTPUT_QUAD_FIT_SMART,
} OUTPUT_QUAD_FIT_MODE;

// Create a quad renderer instance and initialize GL resources.
OUTPUT_QUAD *Output_Quad_Create(void);
// Destroy a quad renderer instance and release its GL resources.
void Output_Quad_Destroy(OUTPUT_QUAD *renderer);

// Upload pixel data into the renderer-owned texture.
void Output_Quad_Upload(
    OUTPUT_QUAD *renderer, const OUTPUT_QUAD_SURFACE_DESC *desc,
    const uint8_t *data);

// Bind an external texture as the source image for rendering.
void Output_Quad_SetExternalTexture(
    OUTPUT_QUAD *renderer, GLuint texture_id, int32_t width, int32_t height,
    bool flip_y);
// Switch back to the renderer-owned texture source.
void Output_Quad_ClearExternalTexture(OUTPUT_QUAD *renderer);

// Set how many times the quad should repeat in X and Y.
void Output_Quad_SetRepeat(OUTPUT_QUAD *renderer, int32_t x, int32_t y);
// Set the source UV rectangle used for texture sampling.
void Output_Quad_SetTextureSize(
    OUTPUT_QUAD *renderer, const OUTPUT_QUAD_TEXTURE_SIZE *size);
// Set visual effect flags applied by the quad shader.
void Output_Quad_SetEffect(OUTPUT_QUAD *renderer, uint32_t effect);

// Set output opacity multiplier.
void Output_Quad_SetOpacity(OUTPUT_QUAD *renderer, float opacity);
// Set brightness scaling multiplier applied in shader.
void Output_Quad_SetBrightnessScale(
    OUTPUT_QUAD *renderer, float brightness_scale);

// Configure fitting mode and source aspect ratio handling.
void Output_Quad_SetFit(
    OUTPUT_QUAD *renderer, OUTPUT_QUAD_FIT_MODE fit_mode, float src_w,
    float src_h);
// Reset fit behavior to stretch with default aspect.
void Output_Quad_ClearFit(OUTPUT_QUAD *renderer);

// Render the quad without forcing blend state changes.
void Output_Quad_Render(OUTPUT_QUAD *renderer);
// Render the quad with alpha blending enabled.
void Output_Quad_RenderWithBlend(OUTPUT_QUAD *renderer);
