#include <trx/config.h>
#include <trx/core/benchmark.h>
#include <trx/core/log.h>
#include <trx/game/camera.h>
#include <trx/game/effects.h>
#include <trx/game/fx.h>
#include <trx/game/game.h>
#include <trx/game/game_strings/table.h>
#include <trx/game/gym.h>
#include <trx/game/lara.h>
#include <trx/game/level.h>
#include <trx/game/lua.h>
#include <trx/game/music.h>
#include <trx/game/objects.h>
#include <trx/game/option.h>
#include <trx/game/output.h>
#include <trx/game/overlay.h>
#include <trx/game/pathing.h>
#include <trx/game/random.h>
#include <trx/game/savegame.h>
#include <trx/game/sound.h>
#include <trx/game/sparks.h>
#include <trx/game/ui.h>

void Level_Unload(void)
{
    Music_ResetTrackFlags();
    Sound_ResetSamples();

    Lara_InitialiseLoad(NO_ITEM);

    Gym_TrackManager_Reset(GYM_TRACK_ASSAULT);
    Gym_TrackManager_Reset(GYM_TRACK_QUAD);
    Creature_Reset();
    Object_Reset();
    Camera_Reset();
    Walkable_Reset();

    Output_SetTimeInGame(0.0f);
    Output_DispatchLevelUnload();

    Sound_StopAll();
    Viewport_AlterFOV(-1, FOV_MODE_GAME);
}

bool Level_Initialise(
    const GF_LEVEL *const level, const GF_SEQUENCE_CONTEXT seq_ctx)
{
    BENCHMARK benchmark = Benchmark_Start();
    LOG_DEBUG("num=%d (%s)", level->num, level->path);
    if (level->type == GFL_DEMO) {
        Random_SeedDraw(0xD371F947);
        Random_SeedControl(0xD371F947);
    }

    Game_SetIsLevelComplete(false);
    if (level->type != GFL_TITLE && level->type != GFL_DEMO) {
        Gym_SetInventoryOpenEnabled(false);
    }
    if (level->type != GFL_TITLE && level->type != GFL_CUTSCENE) {
        Game_SetCurrentLevel(level);
    }
    GF_SetCurrentLevel(level);

    if (level->type != GFL_TITLE) {
        // TODO: move me elsewhere
        RESUME_INFO *const resume = Savegame_GetCurrentInfo(level);
        if (resume != nullptr) {
            resume->stats.timer = 0;
            resume->stats.secret_flags = 0;
            resume->stats.secret_count = 0;
            resume->stats.pickup_count = 0;
            resume->stats.kill_count = 0;
            resume->stats.ammo_hits = 0;
            resume->stats.ammo_used = 0;
            resume->stats.medipacks_used = 0;
            resume->stats.distance_travelled = 0;
        }
    }

    if (level == nullptr) {
        return false;
    }

    Level_Unload();

    Lua_FireEventInt32(LUA_EVENT_BEFORE_LEVEL_FILE, level->num);

    Level_Pipeline_Load(level);

    UI_LoadText();
    Output_SetSkyboxEnabled(Object_Get(O_SKYBOX)->loaded);
    Output_DispatchLevelLoad();

    GameStringTable_Apply(level);

    Effect_InitialiseArray();
    LOT_InitialiseArray();
    FX_Reset();
    FX_Weather_SetWeather(level->weather_type);
    Sparks_Reset();

    Option_Reset();
    Overlay_Reset();
    Overlay_SetHealthBarTimer(100);

    Benchmark_End(&benchmark, nullptr);
    return true;
}
