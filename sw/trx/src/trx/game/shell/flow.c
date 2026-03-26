#include <trx/config.h>
#include <trx/config/presets.h>
#include <trx/core/enum_map.h>
#include <trx/core/log.h>
#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/core/utils.h>
#include <trx/debug.h>
#include <trx/game/catalog/manager.h>
#include <trx/game/clock.h>
#include <trx/game/console.h>
#include <trx/game/events.h>
#include <trx/game/fmv.h>
#include <trx/game/game.h>
#include <trx/game/game_buf.h>
#include <trx/game/game_flow.h>
#include <trx/game/game_strings/entries.h>
#include <trx/game/game_strings/manager.h>
#include <trx/game/gun.h>
#include <trx/game/input.h>
#include <trx/game/inventory_ring.h>
#include <trx/game/items/walkable.h>
#include <trx/game/lara/pose.h>
#include <trx/game/lara/skin.h>
#include <trx/game/lua.h>
#include <trx/game/music.h>
#include <trx/game/option.h>
#include <trx/game/output.h>
#include <trx/game/overlay.h>
#include <trx/game/random.h>
#include <trx/game/replay/test_recorder.h>
#include <trx/game/replay/test_replay.h>
#include <trx/game/rooms.h>
#include <trx/game/savegame.h>
#include <trx/game/shell.h>
#include <trx/game/shell/platform.h>
#include <trx/game/shell/session.h>
#include <trx/game/shell/state.h>
#include <trx/game/sound.h>
#include <trx/game/stats.h>
#include <trx/game/ui/settings.h>
#include <trx/gl/context.h>
#include <trx/version.h>

#include <SDL2/SDL.h>
#include <stdio.h>

static SHELL_SESSION *m_Session = nullptr;
static SDL_Window *m_Window = nullptr;
static char *m_PendingMod = nullptr;

// Flags preserved across mod switches (needed to rebuild args in main()).
static bool m_PrevHeadless = false;
static bool m_PrevQuiet = false;

static void M_CreateGameWindow(void)
{
    if (m_Window != nullptr) {
        return; // Window persists across mod switches
    }
    m_Window = SDL_CreateWindow(
        "TRX", g_Config.window.x, g_Config.window.y, g_Config.window.width,
        g_Config.window.height,
        SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);

    if (m_Window == nullptr) {
        Shell_ExitSystemFmt("Failed to create SDL window: %s", SDL_GetError());
    }
    Shell_EnableThemeSupport(m_Window);
}

static void M_CreateGLContext(void)
{
    if (TRX_GL_Context_GetWindowHandle() != nullptr) {
        return; // GL context persists across mod switches
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    if (!TRX_GL_Context_Attach(m_Window)) {
        Shell_ExitSystem("System Error: cannot attach opengl context");
    }
}

static void M_ShowWindow(void)
{
    Shell_SyncToWindow();
    SDL_ShowWindow(m_Window);
    SDL_RaiseWindow(m_Window);
    Shell_RefreshRendererViewport();
}

static void M_HandleConfigChange(const EVENT *const event, void *const data)
{
    const CONFIG *const old = &g_SavedConfig;
    const CONFIG *const new = &g_Config;
    Shell_HandleConfigChange(old, new);
}

static void M_SetupSDL(void)
{
    SDL_version compiled;
    SDL_VERSION(&compiled);
    LOG_INFO(
        "SDL version: %d.%d.%d", compiled.major, compiled.minor,
        compiled.patch);
    if (SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO) < 0) {
        Shell_ExitSystemFmt("Cannot initialize SDL: %s", SDL_GetError());
    }
}

static void M_SetupGL(void)
{
    // Setup minimum properties of GL context
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
}

static void M_LoadCatalog(
    const CATALOG_CONTEXT context, const char *const filename,
    const bool allow_duplicates)
{
    const char *const path =
        TRXPath_Resolve(TRX_DYNAMIC_PATH_CATALOG, filename);
    if (!Catalog_Load(context, path, allow_duplicates)) {
        Shell_ExitSystemFmt("Failed to load catalogs from %s", path);
    }
}

void Shell_RequestModSwitch(const char *const mod_name)
{
    Memory_FreePointer(&m_PendingMod);
    m_PendingMod = Memory_DupStr(mod_name);
}

const char *Shell_GetPendingMod(void)
{
    return m_PendingMod;
}

void Shell_ClearPendingMod(void)
{
    Memory_FreePointer(&m_PendingMod);
}

bool Shell_GetPrevHeadless(void)
{
    return m_PrevHeadless;
}

bool Shell_GetPrevQuiet(void)
{
    return m_PrevQuiet;
}

const SHELL_ARGS *Shell_GetArgs(void)
{
    ASSERT(m_Session != nullptr);
    return m_Session->args;
}

static void M_InitModules(void)
{
    Shell_SetupHiDPI();
    Shell_SetupLibAV();
    M_SetupSDL();
    M_SetupGL();

    GameString_Init();
    GameStringManager_Init();
    UI_Init();
    Overlay_Init();
    GameEvent_Init();

    GameBuf_Init();
    Random_Seed();

    Clock_Init();
    LUA_Init();
}

static void M_ShutdownModules(void)
{
    if (TestReplay_IsOpened()) {
        TestReplay_Close();
    }
    if (TestRecorder_IsOpened()) {
        TestRecorder_Close();
    }

    Lara_Pose_Shutdown();
    Lara_Skin_Shutdown();

    Console_Shutdown();
    Savegame_Shutdown();

    GF_Shutdown();
    LUA_Shutdown();
    Overlay_Shutdown();
    Option_Shutdown();
    Output_Shutdown();

    Input_Shutdown();
    Music_Shutdown();
    Sound_Shutdown();
    UI_Shutdown();
    GameEvent_Shutdown();

    GameStringManager_Shutdown();
    GameString_Shutdown();
    Walkable_Shutdown();
    Room_Shutdown();
    GameBuf_Shutdown();
    Catalog_Shutdown();
}

static void M_PrepareSystem(void)
{
    SHELL_SESSION *const s = m_Session;
    ASSERT(s != nullptr);
    const char *const test_replay_path = s->args->test_replay_path;

    if (s->args->test_record_path != nullptr
        && s->args->test_replay_path != nullptr) {
        Shell_ExitSystem("Cannot use both --test-record and --test-replay");
    }

    if (test_replay_path != nullptr) {
        // Allow inferring engine version from outer args for replays lacking
        // embedded info (created with the old directory layout).
        g_TRVersion = s->args->engine_version;
        SHELL_ARGS *const tmp_args = TestReplay_Open(test_replay_path);
        if (tmp_args != nullptr) {
            tmp_args->headless = s->args->headless;
            tmp_args->debug_render_performance =
                s->args->debug_render_performance;
            ShellSession_UseArgs(s, tmp_args);
        }
    } else if (s->args->headless) {
        Shell_ExitSystem("--headless can only be used with --test-replay");
    }

    g_TRVersion = s->args->engine_version;
    LOG_INFO("Engine version: %d", g_TRVersion);
    LOG_INFO("Mod: %s", s->args->mod != nullptr ? s->args->mod->name : nullptr);
    if (s->args->engine_version <= 0 || s->args->mod == nullptr) {
        Shell_ExitSystem("No playable mods available.");
    }
    if (s->args->mod->mod_type != MOD_DIRECT_LEVEL) {
        ShellState_RememberLastPlayedMod(s->args->mod->name);
    }

    Config_ApplyDefaultSettings();

    TRXPath_Init(s->args);

    Input_Init();
    Console_Init();
    M_LoadCatalog(CATALOG_OBJECTS, "catalog_objects.csv", false);
    M_LoadCatalog(CATALOG_MUSIC, "catalog_music.csv", false);
    M_LoadCatalog(CATALOG_SAMPLES, "catalog_samples.csv", true);
    M_LoadCatalog(CATALOG_LARA_STATES, "catalog_lara_states.csv", false);
    M_LoadCatalog(CATALOG_LARA_ANIMS, "catalog_lara_anims.csv", false);
    M_LoadCatalog(CATALOG_ITEM_ACTIONS, "catalog_item_actions.csv", false);
    Lara_Pose_Init();
    InvRing_LoadVars(
        TRXPath_Resolve(TRX_DYNAMIC_PATH_COMMON_CONFIG, "inv_ring.json5"));
    Gun_LoadVars(
        TRXPath_Resolve(TRX_DYNAMIC_PATH_COMMON_CONFIG, "weapons.json5"));
    UI_Settings_LoadFromFile(
        TRXPath_Resolve(TRX_DYNAMIC_PATH_COMMON_CONFIG, "ui.json5"));
    Lara_Skin_LoadFromFile(
        TRXPath_Resolve(TRX_DYNAMIC_PATH_COMMON_CONFIG, "outfits.json5"));
    Config_Presets_ScanFiles();

    if (test_replay_path != nullptr) {
        TestReplay_Start();
    } else {
        char *engine_config_path =
            TRXPath_ExpandVars("%config_dir%/TR%tr_version%X.json5");
        if (engine_config_path == nullptr) {
            Shell_ExitSystem("Failed to resolve engine config path");
        }
        Config_Read(engine_config_path, Shell_GetGameFlowPath(s->args->mod));
        Memory_FreePointer(&engine_config_path);

        if (s->args->test_record_path != nullptr) {
            TestRecorder_Open(
                s->args->test_record_path, s->args->original_args);
        }
    }
    Config_SubscribeChanges(M_HandleConfigChange, nullptr);

    Clock_SetSimSpeed(Clock_GetSpeedMultiplier());
    if (!s->args->headless) {
        Sound_Init();
        Music_Init();
        Sound_SetMasterVolume(g_Config.audio.sound_volume);
        Music_SetVolume(g_Config.audio.music_volume);
    } else {
        Clock_DisableWait();
        const int32_t fps = s->args->headless_fps > 0 ? s->args->headless_fps
                                                      : Clock_GetCurrentFPS();
        Clock_EnableHeadlessFixedFPS(fps);
    }
}

SDL_Window *Shell_GetWindow(void)
{
    return m_Window;
}

int32_t Shell_Main(const SHELL_ARGS *const args)
{
    ASSERT(m_Session == nullptr);
    m_Session = ShellSession_Create();

    SHELL_SESSION *const s = m_Session;
    ShellSession_UseArgs(s, args);

    LOG_INFO("Game directory: %s", TRXPath_Get(TRX_PATH_TRX_DIR));

    M_InitModules();
    M_PrepareSystem();
    if (s->args->mod == nullptr) {
        Shell_ExitSystem("No --mod specified.");
        return 1;
    }
    TRXPath_Init(s->args);
    M_CreateGameWindow();
    M_CreateGLContext();
    Output_Init();
    if (!s->args->headless) {
        M_ShowWindow();
    }

    GF_Init();
    GF_LoadFromFile(Shell_GetGameFlowPath(s->args->mod));

    GameStringManager_ClearSourceFiles();
    const char *const common_strings_path = Shell_GetCommonStringsPath();
    if (common_strings_path == nullptr) {
        Shell_ExitSystem("Missing common strings file");
    }
    GameStringManager_AddSourceFile(common_strings_path, false);
    if (s->args->mod->base_mod != nullptr) {
        const char *const base_strings_path =
            Shell_GetBaseGameStringsPath(s->args->mod);
        if (base_strings_path == nullptr) {
            Shell_ExitSystemFmt(
                "Missing base mod strings file for '%s'", s->args->mod->name);
        }
        GameStringManager_AddSourceFile(base_strings_path, false);
    }
    const char *const mod_strings_path = Shell_GetGameStringsPath(s->args->mod);
    if (mod_strings_path == nullptr) {
        Shell_ExitSystemFmt(
            "Missing strings file for selected mod '%s'", s->args->mod->name);
    }
    GameStringManager_AddSourceFile(mod_strings_path, true);
    GameStringManager_DiscoverLanguages();
    GameStringManager_ReloadLanguage(g_Config.language);

    Savegame_Init();
    Savegame_ScanSavedGames();

    // Execute global Lua script if provided
    if (g_GameFlow.main_script_path != nullptr) {
        LUA_RESULT res = Lua_EvalFile(g_GameFlow.main_script_path);
        if (res.code != LUA_OK) {
            LOG_ERROR("Lua main script error: %s", res.message);
        }
        Lua_FreeResult(&res);
    }

    Stats_CalculateMaxStats();
    GF_COMMAND gf_cmd = GF_DoFrontendSequence();

    bool loop_continue = !Shell_IsExiting();
    while (loop_continue) {
        LOG_INFO(
            "action=%s param=%d", ENUM_MAP_TO_STRING(GF_ACTION, gf_cmd.action),
            gf_cmd.param);

        switch (gf_cmd.action) {
        case GF_START_GAME:
        case GF_SELECT_GAME: {
            const int32_t level_num = gf_cmd.param;
            const GF_LEVEL *const level = GF_GetLevel(GFLT_MAIN, level_num);
            const GF_SEQUENCE_CONTEXT seq_ctx =
                gf_cmd.action == GF_SELECT_GAME ? GFSC_SELECT : GFSC_NORMAL;
            if (level != nullptr) {
                gf_cmd = GF_DoLevelSequence(level, seq_ctx);
            }
            break;
        }

        case GF_GLOBE_SELECT:
            gf_cmd = GF_RunGlobeSelect(nullptr);
            break;

        case GF_START_SAVED_GAME: {
            const SAVEGAME_SLOT_REF slot = Savegame_SlotFromParam(gf_cmd.param);
            const int32_t level_num = Savegame_GetLevelNumber(slot);
            if (level_num < 0) {
                LOG_ERROR("Corrupt save file!");
                gf_cmd = (GF_COMMAND) { .action = GF_EXIT_TO_TITLE };
            } else {
                Savegame_BindSlot(slot);
                const GF_LEVEL *const level = GF_GetLevel(GFLT_MAIN, level_num);
                gf_cmd = GF_DoLevelSequence(level, GFSC_SAVED);
            }
            break;
        }

        case GF_RESTART_GAME: {
            const GF_LEVEL *const level = GF_GetLevel(GFLT_MAIN, gf_cmd.param);
            gf_cmd = GF_InterpretSequence(level, GFSC_RESTART, nullptr);
            break;
        }

        case GF_STORY_SO_FAR:
            gf_cmd =
                GF_PlayAvailableStory(Savegame_SlotFromParam(gf_cmd.param));
            break;

        case GF_START_CINE:
            gf_cmd = GF_DoCutsceneSequence(gf_cmd.param);
            break;

        case GF_START_DEMO:
            gf_cmd = GF_DoDemoSequence(gf_cmd.param);
            break;

        case GF_NOOP:
        case GF_LEVEL_COMPLETE:
            gf_cmd = (GF_COMMAND) { .action = GF_EXIT_TO_TITLE };
            break;

        case GF_EXIT_TO_TITLE:
            if (s->args->level_to_play != nullptr) {
                gf_cmd = (GF_COMMAND) { .action = GF_EXIT_GAME };
            } else if (g_GameFlow.title_level == nullptr) {
                Shell_ExitSystem("Missing title level");
            } else {
                gf_cmd = GF_RunTitle();
            }
            break;

        case GF_EXIT_GAME:
        case GF_SWITCH_MOD:
            loop_continue = false;
            break;

        default:
            ASSERT_FAIL_FMT(
                "invalid action (action=%s, param=%d)",
                ENUM_MAP_TO_STRING(GF_ACTION, gf_cmd.action), gf_cmd.param);
        }
    }

    Game_SetCurrentLevel(nullptr);

    if (m_PendingMod != nullptr) {
        if (TestReplay_IsOpened()) {
            TestReplay_Close();
        }
        if (TestRecorder_IsOpened()) {
            TestRecorder_Close();
        }
        // Save flags needed to rebuild args in main() before freeing the
        // session (which owns and will free the args struct).
        m_PrevHeadless = s->args->headless;
        m_PrevQuiet = s->args->quiet;
        M_ShutdownModules();
        ShellSession_Free(m_Session);
        m_Session = nullptr;
        return 0;
    }

    const int32_t replay_exit_code = TestReplay_GetExitCodeOverride();
    return replay_exit_code >= 0 ? replay_exit_code : 0;
}

void Shell_Shutdown(void)
{
    M_ShutdownModules();
    TRX_GL_Context_Detach();
    Log_Shutdown();
    if (m_Session != nullptr) {
        ShellSession_Free(m_Session);
        m_Session = nullptr;
    }
}
