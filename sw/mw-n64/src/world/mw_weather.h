/*
 * MW-N64: Weather and day/night cycle
 *
 * Morrowind has 10 weather types per region, with smooth transitions.
 * Day/night affects ambient light, fog color, and sky color.
 *
 * INTEGRITY RULE: All time constants, color values, and transition
 * rates are from GMST records and region WEAT data in the ESM.
 * Nothing is invented.
 *
 * Time system from OpenMW (apps/openmw/mwworld/weather.cpp):
 *   1 game hour = 10 real seconds (default fTimescale = 30)
 *   Sunrise/sunset times from GMST
 *   Weather transition: 2 game hours (default)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MW_WEATHER_H
#define MW_WEATHER_H

#include <stdint.h>
#include <stdbool.h>

/* Weather types (from OpenMW WeatherManager, exact Morrowind values) */
typedef enum {
    WEATHER_CLEAR = 0,
    WEATHER_CLOUDY = 1,
    WEATHER_FOGGY = 2,
    WEATHER_OVERCAST = 3,
    WEATHER_RAIN = 4,
    WEATHER_THUNDER = 5,
    WEATHER_ASH = 6,
    WEATHER_BLIGHT = 7,
    WEATHER_SNOW = 8,     /* Bloodmoon */
    WEATHER_BLIZZARD = 9, /* Bloodmoon */
    WEATHER_COUNT = 10,
} weather_type_t;

/* Time of day (affects lighting) */
typedef enum {
    TOD_NIGHT = 0,
    TOD_SUNRISE = 1,
    TOD_DAY = 2,
    TOD_SUNSET = 3,
} time_of_day_t;

/* Color (RGB float, interpolated between times) */
typedef struct {
    float r, g, b;
} mw_color_t;

/* Weather state */
typedef struct {
    /* Current time */
    float    game_hour;     /* 0.0 - 24.0 */
    int32_t  game_day;
    int32_t  game_month;
    int32_t  game_year;

    /* Time scale (GMST fTimeScale, default 30) */
    float    time_scale;

    /* Current weather */
    weather_type_t current_weather;
    weather_type_t next_weather;
    float    transition;    /* 0.0 = fully current, 1.0 = fully next */

    /* Computed lighting (interpolated from weather + time of day) */
    mw_color_t ambient;
    mw_color_t sun;
    mw_color_t fog;
    float      fog_depth;

    /* Sun direction (for shadow direction) */
    float    sun_angle;     /* Radians, 0 = horizon, pi/2 = overhead */
} weather_state_t;

/* GMST time constants (exact Morrowind values) */
#define MW_SUNRISE_HOUR     6.0f
#define MW_SUNSET_HOUR      18.0f
#define MW_SUNRISE_DURATION 2.0f
#define MW_SUNSET_DURATION  2.0f
#define MW_DEFAULT_TIMESCALE 30.0f  /* 1 real second = 30 game seconds */

/* -------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------- */

void weather_init(weather_state_t *ws);

/* Advance time by real_dt seconds. Updates game_hour, lighting. */
void weather_update(weather_state_t *ws, float real_dt);

/* Get current time-of-day category */
time_of_day_t weather_get_tod(const weather_state_t *ws);

/* Force weather change (e.g., from script or region transition) */
void weather_set(weather_state_t *ws, weather_type_t type);

#endif /* MW_WEATHER_H */
