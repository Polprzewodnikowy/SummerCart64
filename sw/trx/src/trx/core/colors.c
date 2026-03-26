#include <trx/core/colors.h>

#include <trx/core/utils.h>

#include <math.h>

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

RGBA_8888 Color_RGB888ToRGBA8888_Impl(const RGB_888 color)
{
    return Color_RGB888ToRGBA8888Ex_Impl(color, 255);
}

RGBA_8888 Color_RGB888ToRGBA8888Ex_Impl(
    const RGB_888 color, const uint8_t alpha)
{
    return (RGBA_8888) { color.r, color.g, color.b, alpha };
}

RGBA_F Color_RGBFToRGBAF_Impl(const RGB_F color)
{
    return Color_RGBFToRGBAFEx_Impl(color, 1.0f);
}

RGBA_F Color_RGBFToRGBAFEx_Impl(const RGB_F color, const float alpha)
{
    return (RGBA_F) { color.r, color.g, color.b, alpha };
}

RGBA_8888 Color_ARGB1555ToRGBA8888(const uint16_t argb1555)
{
    // Extract 5-bit values for each ARGB component.
    uint8_t a1 = (argb1555 >> 15) & 0x01;
    uint8_t r5 = (argb1555 >> 10) & 0x1F;
    uint8_t g5 = (argb1555 >> 5) & 0x1F;
    uint8_t b5 = argb1555 & 0x1F;

    // Expand 5-bit color components to 8-bit.
    uint8_t a8 = a1 * 255; // 1-bit alpha (either 0 or 255)
    uint8_t r8 = (r5 << 3) | (r5 >> 2);
    uint8_t g8 = (g5 << 3) | (g5 >> 2);
    uint8_t b8 = (b5 << 3) | (b5 >> 2);

    return (RGBA_8888) {
        .r = r8,
        .g = g8,
        .b = b8,
        .a = a8,
    };
}

RGB_F Color_MixRGBF_Impl(
    const RGB_F color_1, const RGB_F color_2, const float ratio)
{
    return (RGB_F) {
        .r = color_1.r + (color_2.r - color_1.r) * ratio,
        .g = color_1.g + (color_2.g - color_1.g) * ratio,
        .b = color_1.b + (color_2.b - color_1.b) * ratio,
    };
}

RGBA_F Color_MixRGBAF_Impl(
    const RGBA_F color_1, const RGBA_F color_2, const float ratio)
{
    return (RGBA_F) {
        .r = color_1.r + (color_2.r - color_1.r) * ratio,
        .g = color_1.g + (color_2.g - color_1.g) * ratio,
        .b = color_1.b + (color_2.b - color_1.b) * ratio,
        .a = color_1.a + (color_2.a - color_1.a) * ratio,
    };
}

RGB_888 Color_MixRGB888_Impl(
    const RGB_888 color_1, const RGB_888 color_2, const float ratio)
{
    return (RGB_888) {
        .r = (uint8_t)(color_1.r + (color_2.r - color_1.r) * ratio),
        .g = (uint8_t)(color_1.g + (color_2.g - color_1.g) * ratio),
        .b = (uint8_t)(color_1.b + (color_2.b - color_1.b) * ratio),
    };
}

RGBA_8888 Color_MixRGBA8888_Impl(
    const RGBA_8888 color_1, const RGBA_8888 color_2, const float ratio)
{
    return (RGBA_8888) {
        .r = (uint8_t)(color_1.r + (color_2.r - color_1.r) * ratio),
        .g = (uint8_t)(color_1.g + (color_2.g - color_1.g) * ratio),
        .b = (uint8_t)(color_1.b + (color_2.b - color_1.b) * ratio),
        .a = (uint8_t)(color_1.a + (color_2.a - color_1.a) * ratio),
    };
}

RGB_888 Color_HSLToRGB(const float h, const float s, const float l)
{
    float hue = h < 0.0f ? 0.0f : fmodf(h, 360.0f);
    float sat = s;
    float light = l;
    CLAMP(hue, 0.0f, 360.0f);
    CLAMP(sat, 0.0f, 1.0f);
    CLAMP(light, 0.0f, 1.0f);

    const float c = (1.0f - fabsf(2.0f * light - 1.0f)) * sat;
    const float x = c * (1.0f - fabsf(fmodf(hue / 60.0f, 2.0f) - 1.0f));
    const float m = light - c / 2.0f;

    float rp = 0.0f;
    float gp = 0.0f;
    float bp = 0.0f;

    if (hue < 60.0f) {
        rp = c;
        gp = x;
    } else if (hue < 120.0f) {
        rp = x;
        gp = c;
    } else if (hue < 180.0f) {
        gp = c;
        bp = x;
    } else if (hue < 240.0f) {
        gp = x;
        bp = c;
    } else if (hue < 300.0f) {
        rp = x;
        bp = c;
    } else {
        rp = c;
        bp = x;
    }

    return (RGB_888) {
        .r = (uint8_t)roundf((rp + m) * 255.0f),
        .g = (uint8_t)roundf((gp + m) * 255.0f),
        .b = (uint8_t)roundf((bp + m) * 255.0f),
    };
}

void Color_RGBToHSL(
    const RGB_888 color, float *const out_h, float *const out_s,
    float *const out_l)
{
    const float r = color.r / 255.0f;
    const float g = color.g / 255.0f;
    const float b = color.b / 255.0f;
    const float max_c = MAX(r, MAX(g, b));
    const float min_c = MIN(r, MIN(g, b));
    const float delta = max_c - min_c;
    float light = (max_c + min_c) / 2.0f;

    float hue = 0.0f;
    float sat = 0.0f;
    if (delta > 0.0f) {
        if (max_c == r) {
            hue = 60.0f * fmodf((g - b) / delta, 6.0f);
        } else if (max_c == g) {
            hue = 60.0f * (((b - r) / delta) + 2.0f);
        } else {
            hue = 60.0f * (((r - g) / delta) + 4.0f);
        }
        if (hue < 0.0f) {
            hue += 360.0f;
        }
        sat = delta / (1.0f - fabsf(2.0f * light - 1.0f));
    }

    CLAMP(hue, 0.0f, 360.0f);
    CLAMP(sat, 0.0f, 1.0f);
    CLAMP(light, 0.0f, 1.0f);
    *out_h = hue;
    *out_s = sat;
    *out_l = light;
}

static float M_SRGBToLinear(const float c)
{
    if (c <= 0.04045f) {
        return c / 12.92f;
    }
    return powf((c + 0.055f) / 1.055f, 2.4f);
}

static float M_LinearToSRGB(const float c)
{
    if (c <= 0.0031308f) {
        return c * 12.92f;
    }
    return 1.055f * powf(c, 1.0f / 2.4f) - 0.055f;
}

RGB_888 Color_OKLCHToRGB(const float l, const float c, const float h)
{
    float lightness = l;
    float chroma = c;
    float hue = h < 0.0f ? 0.0f : fmodf(h, 360.0f);
    CLAMP(lightness, 0.0f, 1.0f);
    CLAMP(chroma, 0.0f, 1.0f);
    CLAMP(hue, 0.0f, 360.0f);

    const float hue_rad = hue * M_PI / 180.0f;
    const float a = chroma * cosf(hue_rad);
    const float b = chroma * sinf(hue_rad);

    const float l_ = lightness + 0.3963377774f * a + 0.2158037573f * b;
    const float m_ = lightness - 0.1055613458f * a - 0.0638541728f * b;
    const float s_ = lightness - 0.0894841775f * a - 1.2914855480f * b;

    const float l_3 = l_ * l_ * l_;
    const float m_3 = m_ * m_ * m_;
    const float s_3 = s_ * s_ * s_;

    float r_linear =
        +4.0767416621f * l_3 - 3.3077115913f * m_3 + 0.2309699292f * s_3;
    float g_linear =
        -1.2684380046f * l_3 + 2.6097574011f * m_3 - 0.3413193965f * s_3;
    float b_linear =
        -0.0041960863f * l_3 - 0.7034186147f * m_3 + 1.7076147010f * s_3;
    CLAMP(r_linear, 0.0f, 1.0f);
    CLAMP(g_linear, 0.0f, 1.0f);
    CLAMP(b_linear, 0.0f, 1.0f);

    const float r_srgb = M_LinearToSRGB(r_linear);
    const float g_srgb = M_LinearToSRGB(g_linear);
    const float b_srgb = M_LinearToSRGB(b_linear);

    return (RGB_888) {
        .r = (uint8_t)roundf(r_srgb * 255.0f),
        .g = (uint8_t)roundf(g_srgb * 255.0f),
        .b = (uint8_t)roundf(b_srgb * 255.0f),
    };
}

void Color_RGBToOKLCH(
    const RGB_888 color, float *const out_l, float *const out_c,
    float *const out_h)
{
    const float r = M_SRGBToLinear(color.r / 255.0f);
    const float g = M_SRGBToLinear(color.g / 255.0f);
    const float b = M_SRGBToLinear(color.b / 255.0f);

    const float l =
        cbrtf(0.4122214708f * r + 0.5363325363f * g + 0.0514459929f * b);
    const float m =
        cbrtf(0.2119034982f * r + 0.6806995451f * g + 0.1073969566f * b);
    const float s =
        cbrtf(0.0883024619f * r + 0.2817188376f * g + 0.6299787005f * b);

    float lightness = 0.2104542553f * l + 0.7936177850f * m - 0.0040720468f * s;
    const float a = 1.9779984951f * l - 2.4285922050f * m + 0.4505937099f * s;
    const float b_comp =
        0.0259040371f * l + 0.7827717662f * m - 0.8086757660f * s;

    const float chroma = sqrtf(a * a + b_comp * b_comp);
    float hue = atan2f(b_comp, a) * 180.0f / M_PI;
    if (hue < 0.0f) {
        hue += 360.0f;
    }

    CLAMP(lightness, 0.0f, 1.0f);
    *out_l = lightness;
    *out_c = chroma;
    *out_h = hue;
}
