#include <trx/core/enum_map.h>
#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/core/vector.h>
#include <trx/game/console/common.h>
#include <trx/game/console/registry.h>
#include <trx/game/fx/weather.h>
#include <trx/game/game.h>
#include <trx/game/game_flow.h>
#include <trx/game/game_strings/entries.h>

#include <string.h>

static char *M_GetAvailableWeatherTypes(void)
{
    VECTOR *const values = EnumMap_ListValues("WEATHER_TYPE");
    if (values == nullptr) {
        return nullptr;
    }

    size_t total_len = 1;
    const char *const sep = ", ";
    for (int32_t i = 0; i < values->count; i++) {
        const char *const s = *(char **)Vector_Get(values, i);
        total_len += strlen(s) + (i + 1 < values->count ? strlen(sep) : 0);
    }

    char *const result = Memory_Alloc(total_len);
    for (int32_t i = 0; i < values->count; i++) {
        const char *const s = *(char **)Vector_Get(values, i);
        strcat(result, s);
        if (i + 1 < values->count) {
            strcat(result, sep);
        }
    }

    Vector_Free(values);
    return result;
}

static COMMAND_RESULT M_Entrypoint(const COMMAND_CONTEXT *const ctx)
{
    if (GF_GetCurrentLevel() == nullptr
        || GF_GetCurrentLevel()->type == GFL_TITLE) {
        return CR_UNAVAILABLE;
    }

    if (!Game_IsLoaded()) {
        return CR_UNAVAILABLE;
    }

    if (String_IsEmpty(ctx->args)) {
        return CR_BAD_INVOCATION;
    }

    const int32_t weather_type_raw = ENUM_MAP_GET(WEATHER_TYPE, ctx->args, -1);
    if (weather_type_raw == -1) {
        char *available = M_GetAvailableWeatherTypes();
        if (available != nullptr) {
            Console_LogError(
                GS("console/cmd/weather/invalid"), ctx->args, available);
        }
        Memory_FreePointer(&available);
        return CR_FAILURE;
    }

    FX_Weather_SetWeather((WEATHER_TYPE)weather_type_raw);
    Console_Log(GS("console/cmd/weather/set"), ctx->args);
    return CR_SUCCESS;
}

REGISTER_CONSOLE_COMMAND(
    "weather", M_Entrypoint, GS_ID("console/cmd/weather/help"))
