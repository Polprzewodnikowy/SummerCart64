#include <trx/game/game_flow/sequencer_events.h>

#include <trx/config.h>
#include <trx/core/log.h>
#include <trx/debug.h>
#include <trx/game/fmv.h>
#include <trx/game/game.h>
#include <trx/game/game_flow/sequencer.h>
#include <trx/game/game_flow/util.h>
#include <trx/game/game_flow/vars.h>
#include <trx/game/lara.h>
#include <trx/game/lua.h>
#include <trx/game/music.h>
#include <trx/game/objects/creatures/bacon_lara.h>
#include <trx/game/option/passport.h>
#include <trx/game/output.h>
#include <trx/game/phase.h>
#include <trx/game/savegame.h>
#include <trx/game/shell/paths.h>
#include <trx/game/stats.h>
#include <trx/version.h>

#define M_GF_HANDLER(name)                                                     \
    static GF_COMMAND name(                                                    \
        const GF_LEVEL *const level, const GF_SEQUENCE *const sequence,        \
        const int32_t event_idx, const GF_SEQUENCE_CONTEXT seq_ctx,            \
        void *const seq_ctx_arg)

// clang-format off
#define X_EVENT_HANDLER_LIST \
    X(GFS_EXIT_TO_TITLE,     M_HandleExitToTitle)                              \
    X(GFS_LEVEL_COMPLETE,    M_HandleLevelComplete)                            \
    X(GFS_LOOP_GAME,         M_HandlePlayLevel)                                \
    X(GFS_PLAY_CUTSCENE,     M_HandlePlayCutscene)                             \
    X(GFS_PLAY_FMV,          M_HandlePlayFMV)                                  \
    X(GFS_PLAY_MUSIC,        M_HandlePlayMusic)                                \
    X(GFS_ADD_ITEM,          M_HandleInventoryModifier)                        \
    X(GFS_REMOVE_WEAPONS,    M_HandleInventoryModifier)                        \
    X(GFS_REMOVE_AMMO,       M_HandleInventoryModifier)                        \
    X(GFS_REMOVE_MEDIPACKS,  M_HandleInventoryModifier)                        \
    X(GFS_REMOVE_SCIONS,     M_HandleInventoryModifier)                        \
    X(GFS_ADD_SECRET_REWARD, M_HandleInventoryModifier)                        \
    X(GFS_REMOVE_FLARES,     M_HandleInventoryModifier)                        \
    X(GFS_LOADING_SCREEN,    M_HandlePicture)                                  \
    X(GFS_DISPLAY_PICTURE,   M_HandlePicture)                                  \
    X(GFS_LEVEL_STATS,       M_HandleLevelStats)                               \
    X(GFS_TOTAL_STATS,       M_HandleTotalStats)                               \
    X(GFS_GLOBE_SELECT,      M_HandleGlobeSelect)                              \
    X(GFS_SET_START_ANIM,    M_HandleSetStartAnim)                             \
    X(GFS_ENABLE_SUNSET,     M_HandleEnableSunset)                             \
    X(GFS_SETUP_BACON_LARA,  M_HandleSetupBaconLara)                           \
    X(GFS_DISABLE_FLOOR,     M_HandleDisableFloor)
// clang-format on

#define X(id, name) M_GF_HANDLER(name);
X_EVENT_HANDLER_LIST
#undef X

static GF_SEQUENCE_EVENT_HANDLER m_EventHandlers[GFS_NUMBER_OF] = {
#define X(id, name) [id] = name,
    X_EVENT_HANDLER_LIST
#undef X
    // clang-format on
};

static void M_FinishLevelBasic(void)
{
    const GF_LEVEL *const current_level = Game_GetCurrentLevel();

    if (current_level == GF_GetLastLevel()) {
        g_Config.profile.new_game_plus_unlock = true;
        Config_Update();
    }

    RESUME_INFO *const resume = Savegame_GetCurrentInfo(current_level);
    if (resume != nullptr) {
        resume->flags.available = true;
        resume->level_completed = true;
    }
}

static const GF_LEVEL *M_GetCanonicalNextLevel(const GF_LEVEL *const level)
{
    // Canonical order is still used for regular level flow; resume inheritance
    // is non-linear and tracked via RESUME_INFO.prev_level.
    return GF_GetLevelAfter(level);
}

M_GF_HANDLER(M_HandleExitToTitle)
{
    return (GF_COMMAND) { .action = GF_EXIT_TO_TITLE };
}

M_GF_HANDLER(M_HandleLevelComplete)
{
    if (seq_ctx != GFSC_NORMAL) {
        return (GF_COMMAND) { .action = GF_NOOP };
    }
    M_FinishLevelBasic();
    const GF_LEVEL *const current_level = Game_GetCurrentLevel();
    const GF_LEVEL *const next_level = M_GetCanonicalNextLevel(current_level);

    if (next_level == nullptr) {
        return (GF_COMMAND) { .action = GF_NOOP };
    }
    Savegame_PersistGameToCurrentInfo(next_level);
    RESUME_INFO *const next_resume = Savegame_GetCurrentInfo(next_level);
    if (next_resume != nullptr) {
        next_resume->prev_level = current_level->num;
    }
    if (next_level->type == GFL_BONUS && !Stats_CheckAllSecretsCollected()) {
        return (GF_COMMAND) { .action = GF_EXIT_TO_TITLE };
    }
    return (GF_COMMAND) {
        .action = GF_START_GAME,
        .param = next_level->num,
    };
}

M_GF_HANDLER(M_HandlePlayLevel)
{
    GF_COMMAND gf_cmd = { .action = GF_NOOP };

    if (seq_ctx == GFSC_STORY) {
        const int32_t savegame_level_num = (int32_t)(intptr_t)seq_ctx_arg;
        if (savegame_level_num == level->num) {
            return (GF_COMMAND) { .action = GF_EXIT_TO_TITLE };
        } else {
            return (GF_COMMAND) { .action = GF_NOOP };
        }
    }

    if (Lara_GetItem() != nullptr) {
        Lara_Initialise(level);
    }

    if (level->music_track != MX_INACTIVE) {
        Music_Stop();
    }

    Lua_FireEventInt32(LUA_EVENT_AFTER_LEVEL_FILE, level->num);

    // post load
    switch (seq_ctx) {
    case GFSC_SAVED: {
        const SAVEGAME_SLOT_REF slot = Savegame_GetBoundSlot();
        if (!Savegame_Load(slot)) {
            LOG_ERROR("Failed to load save file!");
            Game_SetCurrentLevel(nullptr);
            GF_SetCurrentLevel(nullptr);
            return (GF_COMMAND) { .action = GF_EXIT_TO_TITLE };
        }
        break;
    }

    default:
        if (level->type == GFL_NORMAL || level->type == GFL_BONUS) {
            Savegame_SetInitialVersion(SG_CURRENT_VERSION);
            GF_InventoryModifier_Scan(Game_GetCurrentLevel());
            GF_InventoryModifier_Apply(Game_GetCurrentLevel(), GF_INV_REGULAR);
        }
        break;
    }
    GF_DisableObjectsIfNeeded();

    Lua_FireEventInt32(LUA_EVENT_AFTER_LEVEL_STATE, level->num);

    g_Passport.ask_for_save = g_Config.gameplay.enable_save_crystals
        && seq_ctx == GFSC_NORMAL
        && GF_GetLevelTableType(level->type) == GFLT_MAIN
        && level != GF_GetFirstLevel() && level != GF_GetGymLevel();

    ASSERT(GF_GetCurrentLevel() == level);
    if (level->type == GFL_DEMO) {
        gf_cmd = GF_RunDemo(level->num);
    } else if (level->type == GFL_CUTSCENE) {
        gf_cmd = GF_RunCutscene(level->num);
    } else {
        if (seq_ctx != GFSC_SAVED && level != GF_GetFirstLevel()) {
            Lara_RevertToPistolsIfNeeded();
        }
        gf_cmd = GF_RunGame(level, seq_ctx);
    }
    if (gf_cmd.action == GF_LEVEL_COMPLETE) {
        gf_cmd.action = GF_NOOP;
    }
    return gf_cmd;
}

M_GF_HANDLER(M_HandlePlayCutscene)
{
    GF_COMMAND gf_cmd = { .action = GF_NOOP };
    const GF_SEQUENCE_EVENT *const event = &sequence->events[event_idx];
    const int16_t cutscene_num = (int16_t)(intptr_t)event->data;
    if (seq_ctx != GFSC_SAVED && g_Config.gameplay.enable_cutscenes) {
        gf_cmd = GF_DoCutsceneSequence(cutscene_num);
        if (gf_cmd.action == GF_LEVEL_COMPLETE) {
            gf_cmd.action = GF_NOOP;
        }
    }
    return gf_cmd;
}

M_GF_HANDLER(M_HandlePlayFMV)
{
    GF_COMMAND gf_cmd = { .action = GF_NOOP };
    const GF_SEQUENCE_EVENT *const event = &sequence->events[event_idx];
    const int16_t fmv_id = (int16_t)(intptr_t)event->data;
    if (seq_ctx == GFSC_SAVED) {
        return gf_cmd;
    }
    if (fmv_id < 0 || fmv_id >= g_GameFlow.fmv_count) {
        LOG_ERROR("Invalid FMV number: %d", fmv_id);
        return gf_cmd;
    }
    const GF_FMV *const fmv = &g_GameFlow.fmvs[fmv_id];
    if (fmv->is_legal && !g_Config.gameplay.enable_legal) {
        return gf_cmd;
    }
    if (fmv->is_credit && !g_Config.gameplay.enable_credits) {
        return gf_cmd;
    }
    FMV_Play(fmv->path);
    return gf_cmd;
}

M_GF_HANDLER(M_HandlePlayMusic)
{
    if (seq_ctx != GFSC_STORY) {
        const GF_SEQUENCE_EVENT *const event = &sequence->events[event_idx];
        Music_SetVolume(g_Config.audio.music_volume);
        Music_Play_Direct((int32_t)(intptr_t)event->data, MPM_ONCE);
    }
    return (GF_COMMAND) { .action = GF_NOOP };
}

M_GF_HANDLER(M_HandlePicture)
{
    GF_COMMAND gf_cmd = { .action = GF_NOOP };
    const GF_SEQUENCE_EVENT *const event = &sequence->events[event_idx];
    const GF_SEQUENCE_EVENT *const prev_event =
        event_idx > 0 ? &sequence->events[event_idx - 1] : nullptr;
    const bool is_after_fmv =
        prev_event != nullptr && prev_event->type == GFS_PLAY_FMV;
    if (event->type == GFS_LOADING_SCREEN) {
        if (g_Config.gameplay.loading_screens == LOADING_SCREENS_DISABLED) {
            return gf_cmd;
        } else if (seq_ctx == GFSC_STORY) {
            return gf_cmd;
        } else if (
            g_Config.gameplay.loading_screens == LOADING_SCREENS_NEW_GAMES
            && seq_ctx != GFSC_NORMAL && seq_ctx != GFSC_SELECT) {
            return gf_cmd;
        }
        Music_Stop();
    } else if (seq_ctx == GFSC_SAVED) {
        return gf_cmd;
    }

    GF_DISPLAY_PICTURE_DATA *data = event->data;
    if (data->path == nullptr) {
        return gf_cmd;
    }
    if (data->is_legal && !g_Config.gameplay.enable_legal) {
        return gf_cmd;
    }
    if (data->is_credit && !g_Config.gameplay.enable_credits) {
        return gf_cmd;
    }

    PHASE *const phase = Phase_Picture_Create((PHASE_PICTURE_ARGS) {
        .file_name = data->path,
        .display_time = data->display_time,
        .fade_in_time = data->fade_in_time,
        .fade_out_time = data->fade_out_time,
        .display_time_includes_fades = g_TRVersion >= 2,
        .loading_pic = event->type == GFS_LOADING_SCREEN,
        .block_cross_fade_in = is_after_fmv,
    });
    gf_cmd = PhaseExecutor_Run(phase);
    Phase_Picture_Destroy(phase);
    return gf_cmd;
}

M_GF_HANDLER(M_HandleInventoryModifier)
{
    // handled in GF_InventoryModifier_Apply
    return (GF_COMMAND) { .action = GF_NOOP };
}

M_GF_HANDLER(M_HandleLevelStats)
{
    GF_COMMAND gf_cmd = { .action = GF_NOOP };
    if (seq_ctx != GFSC_NORMAL) {
        return gf_cmd;
    }

    const bool use_bare_style = g_TRVersion == 1
        && g_Config.ui.stat_detail_mode != STAT_DETAIL_MODE_FULL;

    PHASE *const phase = Phase_Stats_Create((PHASE_STATS_ARGS) {
        .background_type = Game_IsInGym() ? BK_TRANSPARENT_MEDIUM
                                          : g_Config.ui.stats_background_style,
        .level_num = -1,
        .show_final_stats = false,
        .use_bare_style = use_bare_style,
    });
    gf_cmd = PhaseExecutor_Run(phase);
    Phase_Stats_Destroy(phase);
    return gf_cmd;
}

M_GF_HANDLER(M_HandleTotalStats)
{
    GF_COMMAND gf_cmd = { .action = GF_EXIT_TO_TITLE };
    if (seq_ctx != GFSC_NORMAL) {
        return gf_cmd;
    }
    if (!g_Config.gameplay.enable_total_stats) {
        return gf_cmd;
    }
    const GF_SEQUENCE_EVENT *const event = &sequence->events[event_idx];
    PHASE *const phase = Phase_Stats_Create((PHASE_STATS_ARGS) {
        .background_type = BK_IMAGE,
        .background_path = event->data,
        .show_final_stats = true,
        .use_bare_style = false,
        .level_num = -1,
    });
    gf_cmd = PhaseExecutor_Run(phase);
    Phase_Stats_Destroy(phase);
    return gf_cmd;
}

M_GF_HANDLER(M_HandleGlobeSelect)
{
    if (seq_ctx != GFSC_NORMAL) {
        return (GF_COMMAND) { .action = GF_NOOP };
    }
    M_FinishLevelBasic();
    const GF_SEQUENCE_EVENT *const event = &sequence->events[event_idx];
    const GF_GLOBE_SELECT_DATA *const data = event->data;
    const GF_COMMAND gf_cmd =
        GF_RunGlobeSelect(data != nullptr ? data->image_path : nullptr);
    if (gf_cmd.action == GF_START_GAME) {
        const GF_LEVEL *const current_level = Game_GetCurrentLevel();
        const GF_LEVEL *const next_level = GF_GetLevel(GFLT_MAIN, gf_cmd.param);
        if (next_level != nullptr) {
            Savegame_PersistGameToCurrentInfo(next_level);
            RESUME_INFO *const next_resume =
                Savegame_GetCurrentInfo(next_level);
            if (next_resume != nullptr) {
                next_resume->prev_level =
                    current_level != nullptr ? current_level->num : -1;
            }
        }
    }
    return gf_cmd;
}

M_GF_HANDLER(M_HandleSetStartAnim)
{
    GF_COMMAND gf_cmd = { .action = GF_NOOP };
    const GF_SEQUENCE_EVENT *const event = &sequence->events[event_idx];
    if (seq_ctx != GFSC_STORY) {
        Lara_SetStartAnimState((LARA_EXTRA_STATE)(intptr_t)event->data);
    }
    return gf_cmd;
}

M_GF_HANDLER(M_HandleEnableSunset)
{
    GF_COMMAND gf_cmd = { .action = GF_NOOP };
    if (seq_ctx != GFSC_STORY) {
        Output_SetSunsetEnabled(true);
    }
    return gf_cmd;
}

M_GF_HANDLER(M_HandleSetupBaconLara)
{
    // TODO: move me to lua!
    if (seq_ctx != GFSC_STORY) {
        const GF_SEQUENCE_EVENT *const event = &sequence->events[event_idx];
        const int32_t anchor_room = (int32_t)(intptr_t)event->data;
        if (!BaconLara_InitialiseAnchor(anchor_room)) {
            LOG_ERROR("Could not anchor Bacon Lara to room %d", anchor_room);
            return (GF_COMMAND) { .action = GF_EXIT_TO_TITLE };
        }
    }
    return (GF_COMMAND) { .action = GF_NOOP };
}

M_GF_HANDLER(M_HandleDisableFloor)
{
    GF_COMMAND gf_cmd = { .action = GF_NOOP };
    if (seq_ctx != GFSC_STORY) {
        const GF_SEQUENCE_EVENT *const event = &sequence->events[event_idx];
        Room_SetAbyssHeight((int16_t)(intptr_t)event->data);
    }
    return gf_cmd;
}

GF_SEQUENCE_EVENT_HANDLER GF_GetSequenceEventHandler(
    const GF_SEQUENCE_EVENT_TYPE event_type)
{
    return m_EventHandlers[event_type];
}
