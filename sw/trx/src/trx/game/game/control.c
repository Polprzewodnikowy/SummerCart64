#include <trx/config.h>
#include <trx/game/camera.h>
#include <trx/game/clock.h>
#include <trx/game/console.h>
#include <trx/game/creature.h>
#include <trx/game/demo.h>
#include <trx/game/effects.h>
#include <trx/game/fx.h>
#include <trx/game/game.h>
#include <trx/game/game_strings/entries.h>
#include <trx/game/gym.h>
#include <trx/game/input.h>
#include <trx/game/interpolation.h>
#include <trx/game/inventory.h>
#include <trx/game/lara.h>
#include <trx/game/lua/events.h>
#include <trx/game/music.h>
#include <trx/game/option/passport.h>
#include <trx/game/output.h>
#include <trx/game/overlay.h>
#include <trx/game/savegame.h>
#include <trx/game/shell.h>
#include <trx/game/sound.h>
#include <trx/game/sparks.h>
#include <trx/game/stats.h>
#include <trx/version.h>

#define M_FRAME_BUFFER(key)                                                    \
    do {                                                                       \
        Shell_ProcessEvents();                                                 \
        Output_BeginScene();                                                   \
        Game_Draw(true);                                                       \
        Input_Update();                                                        \
        Output_EndScene();                                                     \
        Output_FlipScreen();                                                   \
        Clock_WaitTick();                                                      \
    } while (g_Input.key);

int32_t g_OverlayFlag = 0;

bool Game_Start(const GF_LEVEL *const level, const GF_SEQUENCE_CONTEXT seq_ctx)
{
    Game_SetCurrentLevel(level);

    g_OverlayFlag = 1;
    Camera_Initialise();
    Interpolation_Remember();

    Sound_StopAll();
    const bool is_cutscene = level->type == GFL_CUTSCENE;
    if (level->music_track != MX_INACTIVE
        && (is_cutscene || Music_GetCurrentLoopedTrack() == MX_INACTIVE)) {
        Music_Play_Direct(
            level->music_track, is_cutscene ? MPM_ONCE : MPM_LOOP);
    }

    const LUA_EVENT_ARG args[] = {
        { .type = LUA_EVENT_ARG_INT32, .value = { .i32 = level->num } },
        { .type = LUA_EVENT_ARG_BOOL, .value = { .b = seq_ctx == GFSC_SAVED } },
    };
    Lua_FireEventEx(LUA_EVENT_GAME_START, args, 2);
    return true;
}

void Game_End(void)
{
    Savegame_PersistGameToCurrentInfo(Game_GetCurrentLevel());
    Music_Stop();
}

GF_COMMAND Game_Control(const bool demo_mode)
{
    if (g_Passport.ask_for_save) {
        // ask for a save at the start of a level for the save crystals mode
        const GF_COMMAND gf_cmd = GF_ShowInventory(INV_SAVE_CRYSTAL_MODE);
        g_Passport.ask_for_save = false;
        if (gf_cmd.action != GF_NOOP) {
            return gf_cmd;
        }
    }

    const LARA_INFO *const lara = Lara_GetLaraInfo();
    Interpolation_Remember();
    if (!Game_IsInGym() || Gym_TrackManager_IsTimerActive(GYM_TRACK_ASSAULT)
        || Gym_TrackManager_IsTimerActive(GYM_TRACK_QUAD)
        || !Object_Get(O_ASSAULT_DIGITS)->loaded) {
        Stats_UpdateTimer();
    }
    if (Game_IsInGym()) {
        Gym_Control();
    }
    if (g_Config.flow.cheat_keys) {
        Lara_Cheat_CheckKeys();
    }

    if (Game_IsLevelComplete()) {
        Sound_StopAll();
        Music_Stop();
        return (GF_COMMAND) { .action = GF_LEVEL_COMPLETE };
    }

    Input_Update();
    Shell_ProcessInput();
    if (g_InputDB.toggle_photo_mode) {
        return GF_EnterPhotoMode();
    } else if (g_InputDB.pause && lara->death_timer == 0) {
        return GF_PauseGame();
    }
    if (demo_mode) {
        if (g_InputDB.menu_confirm || g_InputDB.menu_back) {
            return (GF_COMMAND) { .action = GF_EXIT_TO_TITLE };
        }
        if (!Demo_UpdateInput()) {
            g_Input = (INPUT_STATE) {};
            g_InputDB = (INPUT_STATE) {};
            return (GF_COMMAND) { .action = GF_EXIT_TO_TITLE };
        }
    }
    Game_ProcessInput();

    if ((g_InputDB.quick_save || g_InputDB.quick_load) && !demo_mode
        && lara->death_timer == 0 && !lara->extra_anim
        && !g_Config.flow.load_save_disabled) {
        bool quick_handled = false;
        if (g_InputDB.quick_save) {
            const SAVEGAME_SLOT_REF slot = Savegame_GetNextQuickSlot();
            if (!Savegame_IsValidSlotRef(slot)) {
                Console_LogError(
                    "%s", GS("general/osd/quick_save_fail_no_slots"));
            } else if (Savegame_Save(slot)) {
                Console_Log("%s", GS("general/osd/quick_save"));
            }
            quick_handled = true;
        } else if (g_InputDB.quick_load) {
            const SAVEGAME_SLOT_REF slot = Savegame_GetBoundSlot();
            if (!Savegame_IsValidSlotRef(slot)) {
                Console_LogError(
                    "%s", GS("general/osd/quick_load_fail_no_bound_slot"));
            } else if (Savegame_IsSlotFree(slot)) {
                Console_LogError(
                    "%s",
                    GS("general/osd/quick_load_fail_unavailable_bound_slot"));
            } else {
                if (slot.pool == SAVEGAME_SLOT_POOL_QUICK) {
                    const int32_t visual_index =
                        Savegame_QuickToVisualIndex(slot);
                    Console_Log(GS("general/osd/quick_load"), visual_index + 1);
                } else {
                    Console_Log(GS("general/osd/load_game"), slot.index + 1);
                }
                return (GF_COMMAND) {
                    .action = GF_START_SAVED_GAME,
                    .param = Savegame_SlotToParam(slot),
                };
            }
            quick_handled = true;
        }

        if (quick_handled) {
            // Prevent mixed bindings (quick + normal save/load on same key)
            // from also opening the passport save/load flow.
            g_Input.save = false;
            g_Input.load = false;
            g_InputDB.save = false;
            g_InputDB.load = false;
        }
    }

    if (lara->death_timer > DEATH_WAIT
        || (lara->death_timer > DEATH_WAIT_INPUT
            && (g_InputDB.menu_confirm || g_InputDB.menu_back)
            && !g_Input.fly_cheat)
        || g_OverlayFlag == 2) {
        if (demo_mode || (g_TRVersion >= 2 && Game_IsInGym())) {
            return (GF_COMMAND) { .action = GF_EXIT_TO_TITLE };
        }
        if (g_OverlayFlag == 2) {
            g_OverlayFlag = 1;
            return GF_ShowInventory(INV_DEATH_MODE);
        } else {
            g_OverlayFlag = 2;
        }
    }

    if ((g_InputDB.option || g_InputDB.load || g_InputDB.save
         || g_OverlayFlag <= 0)
        && lara->death_timer == 0 && !lara->extra_anim) {
        if (g_TRVersion == 1 && g_Camera.type == CAM_CINEMATIC) {
            g_OverlayFlag = 0;
        } else if (g_OverlayFlag > 0) {
            if (g_Config.flow.lockout_option_ring
                && g_Config.flow.load_save_disabled) {
                g_OverlayFlag = 0;
            } else if (g_Input.save) {
                g_OverlayFlag = -2;
            } else if (g_Input.load) {
                g_OverlayFlag = -1;
            } else {
                g_OverlayFlag = 0;
            }
        } else {
            GF_COMMAND gf_cmd;
            if (g_OverlayFlag == -1) {
                gf_cmd = GF_ShowInventory(INV_LOAD_MODE);
            } else if (g_OverlayFlag == -2) {
                gf_cmd = GF_ShowInventory(INV_SAVE_MODE);
            } else {
                gf_cmd = GF_ShowInventory(INV_GAME_MODE);
            }
            g_OverlayFlag = 1;
            if (gf_cmd.action != GF_NOOP) {
                return gf_cmd;
            }
        }
    }

    Output_ResetDynamicLights();

    Sound_ResetAmbient();
    Item_Control();
    Effect_Control();
    Sparks_Control();

    Lara_Control();
    FX_Control();
    Lara_Hair_Control(false);

    Camera_Update();
    ItemAction_RunActive();
    Sound_UpdateEffects();
    Overlay_Animate(1);
    Output_AnimateTextures(1);
    return (GF_COMMAND) { .action = GF_NOOP };
}

void Game_ProcessInput(void)
{
    if (GF_GetCurrentLevel()->type == GFL_DEMO) {
        return;
    }

    if (g_InputDB.use_small_medi && Inv_RequestItem(O_SMALL_MEDIPACK_OPTION)) {
        Lara_UseItem(O_SMALL_MEDIPACK_OPTION);
    }
    if (g_InputDB.use_big_medi && Inv_RequestItem(O_LARGE_MEDIPACK_OPTION)) {
        Lara_UseItem(O_LARGE_MEDIPACK_OPTION);
    }

    if (g_Config.input.enable_buffering_func_keys && Game_IsPlaying()) {
        if (g_Input.toggle_bilinear_filter) {
            M_FRAME_BUFFER(toggle_bilinear_filter);
        } else if (g_Input.toggle_trapezoid_filter) {
            M_FRAME_BUFFER(toggle_trapezoid_filter);
        } else if (g_Input.toggle_fps_counter) {
            M_FRAME_BUFFER(toggle_fps_counter);
        }
    }

    if (g_InputDB.toggle_ui) {
        UI_ToggleState(&g_Config.ui.enable_game_ui);
    }
}
