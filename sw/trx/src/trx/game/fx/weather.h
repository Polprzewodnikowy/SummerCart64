#pragma once

#include <stdint.h>

typedef enum {
    WEATHER_NONE = 0,
    WEATHER_RAIN,
    WEATHER_SNOW,
} WEATHER_TYPE;

void FX_Weather_Reset(void);
void FX_Weather_Control(void);
void FX_Weather_Draw(void);
WEATHER_TYPE FX_Weather_GetWeather(void);
void FX_Weather_SetWeather(WEATHER_TYPE weather_type);
