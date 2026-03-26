#include <trx/game/level/settings.h>

#include <trx/config.h>
#include <trx/game/game_flow.h>
#include <trx/game/game_flow/vars.h>

RGB_888 Level_GetWaterColor(void)
{
    const GF_LEVEL *const level = GF_GetCurrentLevel();
    if (level != nullptr && level->settings.water_color.is_present) {
        return level->settings.water_color.value;
    }
    return g_Config.visuals.water_color;
}

RGBA_8888 Level_GetFogColor(void)
{
    const GF_LEVEL *const level = GF_GetCurrentLevel();
    RGB_888 color = { 0, 0, 0 };
    uint8_t alpha = 255;
    if (level != nullptr && level->settings.fog_transparency.is_present
        && level->settings.fog_transparency.value) {
        alpha = 0;
    } else if (level != nullptr && level->settings.fog_color.is_present) {
        color = level->settings.fog_color.value;
    } else if (g_Config.visuals.fog_transparency) {
        alpha = 0;
    } else {
        color = g_Config.visuals.fog_color;
    }
    return (RGBA_8888) { .r = color.r, .g = color.g, .b = color.b, .a = alpha };
}

float Level_GetFogStart(void)
{
    const GF_LEVEL *const level = GF_GetCurrentLevel();
    if (level != nullptr && level->settings.fog_start.is_present) {
        return level->settings.fog_start.value;
    }
    return g_Config.visuals.fog_start;
}

float Level_GetFogEnd(void)
{
    const GF_LEVEL *const level = GF_GetCurrentLevel();
    if (level != nullptr && level->settings.fog_end.is_present) {
        return level->settings.fog_end.value;
    }
    return g_Config.visuals.fog_end;
}

bool Level_HasColdWater(void)
{
    const GF_LEVEL *const level = GF_GetCurrentLevel();
    if (level != nullptr && level->settings.cold_water.is_present) {
        return level->settings.cold_water.value;
    }

    if (g_GameFlow.settings.cold_water.is_present) {
        return g_GameFlow.settings.cold_water.value;
    }

    return false;
}

GF_DEATH_TILE Level_GetDeathTile(void)
{
    const GF_LEVEL *const level = GF_GetCurrentLevel();
    if (level != nullptr && level->settings.death_tile.is_present) {
        return level->settings.death_tile.value;
    }

    if (g_GameFlow.settings.death_tile.is_present) {
        return g_GameFlow.settings.death_tile.value;
    }

    return GF_DEATH_TILE_LAVA;
}
