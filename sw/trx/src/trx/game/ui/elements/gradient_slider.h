#pragma once

#include <trx/core/colors.h>

#include <stdint.h>

typedef struct {
    float width;
    float value;
    int32_t stop_count;
    const RGB_888 *stops;
} UI_GRADIENT_SLIDER_SETTINGS;

void UI_GradientSlider(UI_GRADIENT_SLIDER_SETTINGS settings);
