#pragma once

#include <stdint.h>

typedef enum {
    UI_SCALER_TARGET_GENERIC,
    UI_SCALER_TARGET_BAR,
    UI_SCALER_TARGET_TEXT,
    UI_SCALER_TARGET_ASSAULT_DIGITS,
} UI_SCALER_TARGET;

double UI_Scaler_GetScale(const UI_SCALER_TARGET target);
float UI_Scaler_Calc(float unit, UI_SCALER_TARGET target);
float UI_Scaler_CalcInverse(float unit, UI_SCALER_TARGET target);
