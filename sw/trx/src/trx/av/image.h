#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} IMAGE_PIXEL;

typedef struct {
    int32_t width;
    int32_t height;
    IMAGE_PIXEL *data;
} IMAGE;

typedef enum {
    IMAGE_FIT_STRETCH,
    IMAGE_FIT_CROP,
    IMAGE_FIT_LETTERBOX,
    IMAGE_FIT_SMART,
} IMAGE_FIT_MODE;

IMAGE *Image_Create(int width, int height);

IMAGE *Image_CreateFromFile(const char *path);

IMAGE *Image_CreateFromFileInto(
    const char *path, int32_t target_width, int32_t target_height,
    IMAGE_FIT_MODE fit_mode);

void Image_Free(IMAGE *image);

bool Image_GetFileInfo(const char *path, int32_t *width, int32_t *height);

bool Image_SaveToFile(const IMAGE *image, const char *path);

IMAGE *Image_Scale(
    const IMAGE *source_image, size_t target_width, size_t target_height,
    IMAGE_FIT_MODE fit_mode);
