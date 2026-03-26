/*
 * MW-N64: Weather and day/night — implementation
 *
 * Time advancement and lighting interpolation.
 * Color values for each weather type and time of day come from
 * GMST records in the ESM.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "mw_weather.h"
#include <string.h>

/* -------------------------------------------------------------------
 * Default clear-weather colors (from Morrowind GMST records)
 *
 * These are the "Clear" weather row. Full implementation reads
 * all 10 weather types from ESM GMST records at init time.
 * ------------------------------------------------------------------- */

/* Ambient light for Clear weather by time of day */
static const mw_color_t clear_ambient[] = {
    [TOD_NIGHT]   = {0.09f, 0.09f, 0.12f},
    [TOD_SUNRISE] = {0.30f, 0.22f, 0.20f},
    [TOD_DAY]     = {0.53f, 0.52f, 0.50f},
    [TOD_SUNSET]  = {0.30f, 0.22f, 0.20f},
};

/* Sun light */
static const mw_color_t clear_sun[] = {
    [TOD_NIGHT]   = {0.00f, 0.00f, 0.00f},
    [TOD_SUNRISE] = {0.60f, 0.40f, 0.30f},
    [TOD_DAY]     = {1.00f, 0.90f, 0.80f},
    [TOD_SUNSET]  = {0.60f, 0.40f, 0.30f},
};

/* Fog color */
static const mw_color_t clear_fog[] = {
    [TOD_NIGHT]   = {0.09f, 0.09f, 0.12f},
    [TOD_SUNRISE] = {0.60f, 0.45f, 0.35f},
    [TOD_DAY]     = {0.75f, 0.75f, 0.73f},
    [TOD_SUNSET]  = {0.60f, 0.45f, 0.35f},
};

static mw_color_t lerp_color(mw_color_t a, mw_color_t b, float t) {
    mw_color_t r;
    r.r = a.r + (b.r - a.r) * t;
    r.g = a.g + (b.g - a.g) * t;
    r.b = a.b + (b.b - a.b) * t;
    return r;
}

void weather_init(weather_state_t *ws) {
    memset(ws, 0, sizeof(*ws));
    ws->game_hour = 9.0f;  /* Start at 9 AM */
    ws->game_day = 16;     /* 16 Last Seed — Morrowind start date */
    ws->game_month = 8;    /* Last Seed */
    ws->game_year = 427;   /* 3E 427 */
    ws->time_scale = MW_DEFAULT_TIMESCALE;
    ws->current_weather = WEATHER_CLEAR;
    ws->next_weather = WEATHER_CLEAR;
}

time_of_day_t weather_get_tod(const weather_state_t *ws) {
    float h = ws->game_hour;
    if (h < MW_SUNRISE_HOUR) return TOD_NIGHT;
    if (h < MW_SUNRISE_HOUR + MW_SUNRISE_DURATION) return TOD_SUNRISE;
    if (h < MW_SUNSET_HOUR) return TOD_DAY;
    if (h < MW_SUNSET_HOUR + MW_SUNSET_DURATION) return TOD_SUNSET;
    return TOD_NIGHT;
}

void weather_update(weather_state_t *ws, float real_dt) {
    /* Advance game time */
    float game_dt = real_dt * ws->time_scale / 3600.0f;  /* Convert to game hours */
    ws->game_hour += game_dt;
    while (ws->game_hour >= 24.0f) {
        ws->game_hour -= 24.0f;
        ws->game_day++;
        /* Month/year rollover simplified */
        if (ws->game_day > 30) {
            ws->game_day = 1;
            ws->game_month++;
            if (ws->game_month > 12) {
                ws->game_month = 1;
                ws->game_year++;
            }
        }
    }

    /* Compute time-of-day interpolation factor */
    float h = ws->game_hour;
    time_of_day_t tod_a, tod_b;
    float tod_t = 0.0f;

    if (h < MW_SUNRISE_HOUR) {
        tod_a = TOD_NIGHT; tod_b = TOD_NIGHT; tod_t = 0.0f;
    } else if (h < MW_SUNRISE_HOUR + MW_SUNRISE_DURATION) {
        tod_a = TOD_NIGHT; tod_b = TOD_SUNRISE;
        tod_t = (h - MW_SUNRISE_HOUR) / MW_SUNRISE_DURATION;
    } else if (h < MW_SUNRISE_HOUR + MW_SUNRISE_DURATION + 1.0f) {
        tod_a = TOD_SUNRISE; tod_b = TOD_DAY;
        tod_t = (h - MW_SUNRISE_HOUR - MW_SUNRISE_DURATION) / 1.0f;
    } else if (h < MW_SUNSET_HOUR) {
        tod_a = TOD_DAY; tod_b = TOD_DAY; tod_t = 0.0f;
    } else if (h < MW_SUNSET_HOUR + MW_SUNSET_DURATION) {
        tod_a = TOD_DAY; tod_b = TOD_SUNSET;
        tod_t = (h - MW_SUNSET_HOUR) / MW_SUNSET_DURATION;
    } else if (h < MW_SUNSET_HOUR + MW_SUNSET_DURATION + 1.0f) {
        tod_a = TOD_SUNSET; tod_b = TOD_NIGHT;
        tod_t = (h - MW_SUNSET_HOUR - MW_SUNSET_DURATION) / 1.0f;
    } else {
        tod_a = TOD_NIGHT; tod_b = TOD_NIGHT; tod_t = 0.0f;
    }

    /* Interpolate lighting for current weather (Clear for now) */
    ws->ambient = lerp_color(clear_ambient[tod_a], clear_ambient[tod_b], tod_t);
    ws->sun = lerp_color(clear_sun[tod_a], clear_sun[tod_b], tod_t);
    ws->fog = lerp_color(clear_fog[tod_a], clear_fog[tod_b], tod_t);

    /* Sun angle for shadows */
    if (h >= MW_SUNRISE_HOUR && h <= MW_SUNSET_HOUR) {
        float day_fraction = (h - MW_SUNRISE_HOUR) / (MW_SUNSET_HOUR - MW_SUNRISE_HOUR);
        ws->sun_angle = day_fraction * 3.14159f;  /* 0 at sunrise, π at sunset */
    } else {
        ws->sun_angle = 0.0f;
    }

    /* Weather transition (simplified — full version uses per-region chances) */
    if (ws->transition > 0.0f && ws->next_weather != ws->current_weather) {
        ws->transition += game_dt / 2.0f;  /* 2 game hours per transition */
        if (ws->transition >= 1.0f) {
            ws->current_weather = ws->next_weather;
            ws->transition = 0.0f;
        }
    }
}

void weather_set(weather_state_t *ws, weather_type_t type) {
    if (type == ws->current_weather) return;
    ws->next_weather = type;
    ws->transition = 0.001f;  /* Start transition */
}
