#pragma once

#include <trx/core/colors.h>

#include <stddef.h>
#include <stdint.h>

#define UI_BAR_COLOR_STEPS 5

typedef enum {
    UI_BAR_LARA_HP,
    UI_BAR_LARA_HP_POISON,
    UI_BAR_LARA_AIR,
    UI_BAR_LARA_STAMINA,
    UI_BAR_LARA_EXPOSURE,
    UI_BAR_ENEMY_HP,
    UI_BAR_ALLY_HP,
    UI_BAR_PROGRESS,
    UI_BAR_NUMBER_OF,
} UI_BAR_TYPE;

typedef enum {
    UI_BAR_THEME_PC_KIND,
    UI_BAR_THEME_PS1_KIND,
} UI_BAR_THEME_KIND;

typedef struct {
    UI_BAR_THEME_KIND kind;
    float basic_scale;
    RGBA_8888 border_light;
    RGBA_8888 border_dark;
    RGBA_8888 border_tl;
    RGBA_8888 border_tr;
    RGBA_8888 border_bl;
    RGBA_8888 border_br;
    RGBA_8888 ramp[UI_BAR_COLOR_STEPS];
    RGBA_8888 ramp_left[UI_BAR_COLOR_STEPS];
    RGBA_8888 ramp_right[UI_BAR_COLOR_STEPS];
} UI_BAR_THEME;

typedef struct {
    RGBA_8888 background[2]; // TS_BACKGROUND: [top, bottom]
    RGBA_8888 background_heavy[2]; // TS_BACKGROUND_HEAVY: [top, bottom]
    RGBA_8888 outline_light;
    RGBA_8888 outline_dark;
} UI_MENU_COLORS_PC;

typedef struct {
    RGBA_8888 background_edge;
    RGBA_8888 background_center;
    RGBA_8888 background_heavy_edge;
    RGBA_8888 background_heavy_center;
    RGBA_8888 heading_edge;
    RGBA_8888 heading_center;
    RGBA_8888 requested_edge;
    RGBA_8888 requested_center;
    RGBA_8888 requested_outline_ch;
    RGBA_8888 requested_outline_cv;
    RGBA_8888 requested_outline_edge;
    RGBA_8888 outline_tl;
    RGBA_8888 outline_tr;
    RGBA_8888 outline_bl;
    RGBA_8888 outline_br;
    RGBA_8888 heading_outline;
} UI_MENU_COLORS_PS1;

void UI_Settings_LoadFromFile(const char *path);

const UI_BAR_THEME *UI_Settings_GetBarTheme(UI_BAR_TYPE type);
bool UI_Settings_IsCurrentBarLookPS1(void);

const UI_MENU_COLORS_PC *UI_Settings_GetMenuColorsPC(void);
const UI_MENU_COLORS_PS1 *UI_Settings_GetMenuColorsPS1(void);
