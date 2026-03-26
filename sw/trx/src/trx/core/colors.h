#pragma once

#include <stdint.h>

typedef struct {
    float r, g, b;
} RGB_F;

typedef struct {
    float r, g, b, a;
} RGBA_F;

typedef struct {
    uint8_t r, g, b;
} RGB_888;

typedef struct {
    uint8_t r, g, b, a;
} RGBA_8888;

#define COLOR_RGBA_8888_BLACK ((RGBA_8888) { 0x00, 0x00, 0x00, 0xFF })
#define COLOR_RGBA_8888_WHITE ((RGBA_8888) { 0xFF, 0xFF, 0xFF, 0xFF })
#define COLOR_RGB_888_BLACK ((RGB_888) { 0x00, 0x00, 0x00 })
#define COLOR_RGB_888_WHITE ((RGB_888) { 0xFF, 0xFF, 0xFF })
#define COLOR_RGB_F_WHITE ((RGB_F) { 1.0f, 1.0f, 1.0f })

RGBA_8888 Color_RGB888ToRGBA8888_Impl(RGB_888 color);
RGBA_8888 Color_RGB888ToRGBA8888Ex_Impl(RGB_888 color, uint8_t alpha);
RGBA_F Color_RGBFToRGBAF_Impl(RGB_F color);
RGBA_F Color_RGBFToRGBAFEx_Impl(RGB_F color, float alpha);
RGBA_8888 Color_ARGB1555ToRGBA8888(uint16_t argb1555);
RGB_F Color_MixRGBF_Impl(RGB_F color_1, RGB_F color_2, float ratio);
RGBA_F Color_MixRGBAF_Impl(RGBA_F color_1, RGBA_F color_2, float ratio);
RGB_888 Color_MixRGB888_Impl(RGB_888 color_1, RGB_888 color_2, float ratio);
RGBA_8888 Color_MixRGBA8888_Impl(
    RGBA_8888 color_1, RGBA_8888 color_2, float ratio);

#define Color_RGBToRGBA(color)                                                 \
    _Generic(                                                                  \
        (color),                                                               \
        RGB_888: Color_RGB888ToRGBA8888_Impl,                                  \
        RGB_F: Color_RGBFToRGBAF_Impl)(color)

#define Color_RGBToRGBAEx(color, alpha)                                        \
    _Generic(                                                                  \
        (color),                                                               \
        RGB_888: Color_RGB888ToRGBA8888Ex_Impl,                                \
        RGB_F: Color_RGBFToRGBAFEx_Impl)(color, alpha)

#define Color_Mix(color_1, color_2, ratio)                                     \
    _Generic(                                                                  \
        (color_1),                                                             \
        RGB_F: Color_MixRGBF_Impl,                                             \
        RGBA_F: Color_MixRGBAF_Impl,                                           \
        RGB_888: Color_MixRGB888_Impl,                                         \
        RGBA_8888: Color_MixRGBA8888_Impl)(color_1, color_2, ratio)

RGB_888 Color_HSLToRGB(float h, float s, float l);
void Color_RGBToHSL(RGB_888 color, float *out_h, float *out_s, float *out_l);
RGB_888 Color_OKLCHToRGB(float l, float c, float h);
void Color_RGBToOKLCH(RGB_888 color, float *out_l, float *out_c, float *out_h);
