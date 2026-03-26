#include <trx/game/inventory_ring/control.h>

#include <trx/config.h>
#include <trx/core/memory.h>
#include <trx/game/camera.h>
#include <trx/game/console.h>
#include <trx/game/game.h>
#include <trx/game/game_flow.h>
#include <trx/game/game_strings/entries.h>
#include <trx/game/gun.h>
#include <trx/game/gym.h>
#include <trx/game/input.h>
#include <trx/game/interpolation.h>
#include <trx/game/inventory.h>
#include <trx/game/inventory_ring/priv.h>
#include <trx/game/inventory_ring/vars.h>
#include <trx/game/lara.h>
#include <trx/game/music.h>
#include <trx/game/option.h>
#include <trx/game/option/examine.h>
#include <trx/game/option/globe_select.h>
#include <trx/game/option/passport.h>
#include <trx/game/option/stats.h>
#include <trx/game/output/overlay.h>
#include <trx/game/overlay.h>
#include <trx/game/savegame.h>
#include <trx/game/shell.h>
#include <trx/game/sound.h>
#include <trx/game/stats.h>
#include <trx/version.h>

#define M_INV_RING_FADE_TIME_FAST                                              \
    (INV_RING_CLOSE_FRAMES / INV_RING_FRAMES / (float)LOGIC_FPS)
#define M_INV_RING_FADE_TIME_TO_BLACK 0.25f
#define M_RING_SWITCH_FRAMES (96 / 2)
#define M_SELECTING_FRAMES (32 / 2)

static CLOCK_TIMER m_DemoTimer = { .type = CLOCK_TIMER_SIM };
static int32_t m_StartLevel;
static OBJECT_ID m_InvChosen = NO_OBJECT;
static INV_RING *m_ActiveRing = nullptr;

INV_RING *InvRing_GetActiveRing(void)
{
    return m_ActiveRing;
}

static void M_ShowAmmoQuantity(const char *const fmt, const int32_t qty)
{
    if (!Game_IsBonusFlagSet(GBF_NGPLUS)) {
        InvRing_ShowItemQuantity(fmt, qty);
    }
}

static void M_RingIsOpen(INV_RING *const ring)
{
    InvRing_ShowHeader(ring);
}

static void M_RingIsNotOpen(INV_RING *const ring)
{
    InvRing_RemoveHeader();
    InvRing_ShowExamine(NO_OBJECT, false);
}

static void M_RingNotActive(
    const INV_RING *const ring, const INVENTORY_ITEM *const inv_item)
{
    InvRing_ShowItemName(inv_item);

    const LARA_INFO *const lara = Lara_GetLaraInfo();
    const int32_t qty = Inv_RequestItem(inv_item->object_id);

    switch (inv_item->object_id) {
    case O_SHOTGUN_OPTION:
        M_ShowAmmoQuantity(
            g_TRVersion == 1 ? "%5d \\{ammo shotgun}" : "%5d",
            lara->shotgun_ammo.ammo / Gun_GetAmmoClipCount(LGT_SHOTGUN));
        break;
    case O_MAGNUM_OPTION:
        M_ShowAmmoQuantity(
            g_TRVersion == 1 ? "%5d \\{ammo magnums}" : "%5d",
            lara->magnum_ammo.ammo);
        break;
    case O_AUTOS_OPTION:
        M_ShowAmmoQuantity("%5d", lara->autos_ammo.ammo);
        break;
    case O_DESERT_EAGLE_OPTION:
        M_ShowAmmoQuantity("%5d", lara->desert_eagle_ammo.ammo);
        break;
    case O_UZI_OPTION:
        M_ShowAmmoQuantity(
            g_TRVersion == 1 ? "%5d \\{ammo uzis}" : "%5d",
            lara->uzi_ammo.ammo);
        break;
    case O_HARPOON_OPTION:
        M_ShowAmmoQuantity("%5d", lara->harpoon_ammo.ammo);
        break;
    case O_M16_OPTION:
        M_ShowAmmoQuantity("%5d", lara->m16_ammo.ammo);
        break;
    case O_MP5_OPTION:
        M_ShowAmmoQuantity("%5d", lara->mp5_ammo.ammo);
        break;
    case O_GRENADE_GUN_OPTION:
        M_ShowAmmoQuantity("%5d", lara->grenade_ammo.ammo);
        break;
    case O_ROCKET_GUN_OPTION:
        M_ShowAmmoQuantity("%5d", lara->rocket_ammo.ammo);
        break;

    case O_SHOTGUN_AMMO_OPTION:
        M_ShowAmmoQuantity("%d", qty * Gun_GetAmmoShellCount(LGT_SHOTGUN));
        break;
    case O_MAGNUM_AMMO_OPTION:
    case O_AUTOS_AMMO_OPTION:
    case O_DESERT_EAGLE_AMMO_OPTION:
    case O_UZI_AMMO_OPTION:
    case O_HARPOON_AMMO_OPTION:
    case O_M16_AMMO_OPTION:
    case O_MP5_AMMO_OPTION:
        M_ShowAmmoQuantity("%d", qty * 2);
        break;
    case O_GRENADE_AMMO_OPTION:
    case O_ROCKET_AMMO_OPTION:
    case O_FLAREBOX_OPTION:
        M_ShowAmmoQuantity("%d", qty);
        break;

    case O_SMALL_MEDIPACK_OPTION:
    case O_LARGE_MEDIPACK_OPTION:
        Overlay_ForceHealthBar(true);
        if (qty > 1) {
            InvRing_ShowItemQuantity("%d", qty);
        }
        break;

    case O_PUZZLE_OPTION_1:
    case O_PUZZLE_OPTION_2:
    case O_PUZZLE_OPTION_3:
    case O_PUZZLE_OPTION_4:
    case O_KEY_OPTION_1:
    case O_KEY_OPTION_2:
    case O_KEY_OPTION_3:
    case O_KEY_OPTION_4:
    case O_QUEST_OPTION_1:
    case O_QUEST_OPTION_2:
    case O_QUEST_OPTION_3:
    case O_QUEST_OPTION_4:
    case O_PICKUP_OPTION_1:
    case O_PICKUP_OPTION_2:
    case O_LEADBAR_OPTION:
    case O_SCION_OPTION:
        if (qty > 1) {
            InvRing_ShowItemQuantity("%d", qty);
        }
        break;

    default:
        break;
    }

    InvRing_ShowExamine(
        inv_item->object_id,
        ring->status == RNG_OPEN
            && Option_Examine_CanExamine(inv_item->object_id));
}

static void M_RingActive(void)
{
    InvRing_RemoveItemTexts();
    InvRing_ShowExamine(NO_OBJECT, false);
}

static bool M_AnimateInventoryItem(INVENTORY_ITEM *const inv_item)
{
    if (inv_item->current_frame == inv_item->goal_frame) {
        InvRing_SelectMeshes(inv_item);
        return false;
    }

    if (inv_item->anim_count > 0) {
        inv_item->anim_count--;
    } else {
        inv_item->anim_count = inv_item->anim_speed;
        inv_item->current_frame += inv_item->anim_direction;
        if (inv_item->current_frame >= inv_item->frames_total) {
            inv_item->current_frame = 0;
        } else if (inv_item->current_frame < 0) {
            inv_item->current_frame = inv_item->frames_total - 1;
        }
    }

    InvRing_SelectMeshes(inv_item);
    return true;
}

static GF_COMMAND M_Finish(INV_RING *const ring, const bool apply_changes)
{
    // TODO: Make this function not have any side effects.
    // Consider adding new GF_ constants, but research other solutions first.

    if (ring->mode == INV_GLOBE_SELECT_MODE) {
        if (ring->globe_select.confirmed && ring->globe_select.selection >= 0
            && ring->globe_select.selection < MAX_GLOBE_ZONES) {
            const int32_t start_level_num =
                ring->globe_select
                    .start_level_num[ring->globe_select.selection];
            if (start_level_num >= 0) {
                return (GF_COMMAND) {
                    .action = GF_START_GAME,
                    .param = start_level_num,
                };
            }
        }
        return (GF_COMMAND) { .action = GF_EXIT_TO_TITLE };
    }

    if (m_StartLevel != -1) {
        return (GF_COMMAND) {
            .action = GF_SELECT_GAME,
            .param = m_StartLevel,
        };
    }

    if (Shell_IsExiting()) {
        return (GF_COMMAND) { .action = GF_EXIT_GAME };
    } else if (GF_GetOverrideCommand().action != GF_NOOP) {
        return GF_GetOverrideCommand();
    } else if (ring->is_demo_needed) {
        return (GF_COMMAND) { .action = GF_START_DEMO, .param = -1 };
    }

    switch (m_InvChosen) {
    case O_PASSPORT_OPTION:
        switch (g_Passport.select_action) {
        case PASSPORT_ACTION_LOAD_GAME: {
            if (apply_changes) {
                Inv_RemoveAllItems();
            }
            return (GF_COMMAND) {
                .action = GF_START_SAVED_GAME,
                .param = Savegame_SlotToParam(g_Passport.select_save_slot),
            };
        }

        case PASSPORT_ACTION_NEW_GAME:
            if (apply_changes) {
                Savegame_InitCurrentInfo();
            }
            Savegame_UnbindSlot();
            return (GF_COMMAND) {
                .action = GF_START_GAME,
                .param = g_Passport.select_level,
            };

        case PASSPORT_ACTION_SWITCH_MOD:
            return (GF_COMMAND) { .action = GF_SWITCH_MOD };

        case PASSPORT_ACTION_SAVE_GAME: {
            if (apply_changes) {
                Savegame_Save(g_Passport.select_save_slot);
            }
            return (GF_COMMAND) { .action = GF_NOOP };
        }

        case PASSPORT_ACTION_RESTART:
            return (GF_COMMAND) {
                .action = GF_RESTART_GAME,
                .param = Game_GetCurrentLevel()->num,
            };

        case PASSPORT_ACTION_EXIT_TO_TITLE:
            return (GF_COMMAND) { .action = GF_EXIT_TO_TITLE };

        case PASSPORT_ACTION_EXIT_GAME:
            return (GF_COMMAND) { .action = GF_EXIT_GAME };

        case PASSPORT_ACTION_SELECT_LEVEL:
            return (GF_COMMAND) {
                .action = GF_SELECT_GAME,
                .param = g_Passport.select_level,
            };

        case PASSPORT_ACTION_GLOBE_SELECT:
            return (GF_COMMAND) {
                .action = GF_GLOBE_SELECT,
                .param = g_Passport.select_level,
            };

        case PASSPORT_ACTION_STORY_SO_FAR:
            return (GF_COMMAND) {
                .action = GF_STORY_SO_FAR,
                .param = Savegame_SlotToParam(g_Passport.select_save_slot),
            };
        }
        break;

    case O_PHOTO_OPTION:
        if (apply_changes) {
            Savegame_UnbindSlot();
        }
        if (GF_GetGymLevel() != nullptr) {
            return (GF_COMMAND) {
                .action = GF_START_GAME,
                .param = GF_GetGymLevel()->num,
            };
        }
        break;

    case O_PISTOL_OPTION:
    case O_SHOTGUN_OPTION:
    case O_MAGNUM_OPTION:
    case O_AUTOS_OPTION:
    case O_DESERT_EAGLE_OPTION:
    case O_UZI_OPTION:
    case O_HARPOON_OPTION:
    case O_M16_OPTION:
    case O_MP5_OPTION:
    case O_GRENADE_GUN_OPTION:
    case O_ROCKET_GUN_OPTION:
    case O_SMALL_MEDIPACK_OPTION:
    case O_LARGE_MEDIPACK_OPTION:
    case O_FLAREBOX_OPTION:
    case O_KEY_OPTION_1:
    case O_KEY_OPTION_2:
    case O_KEY_OPTION_3:
    case O_KEY_OPTION_4:
    case O_PUZZLE_OPTION_1:
    case O_PUZZLE_OPTION_2:
    case O_PUZZLE_OPTION_3:
    case O_PUZZLE_OPTION_4:
    case O_LEADBAR_OPTION:
    case O_SCION_OPTION:
        if (apply_changes) {
            Lara_UseItem(m_InvChosen);
        }
        break;

    default:
        break;
    }

    return (GF_COMMAND) { .action = GF_NOOP };
}

static bool M_CheckDemoTimer(const INV_RING *const ring)
{
    if (!g_Config.gameplay.enable_demo
        || GF_GetLevelTable(GFLT_DEMOS)->count == 0) {
        return false;
    }

    if (ring->mode != INV_TITLE_MODE || InputState_IsAnyPressed(g_Input)
        || InputState_IsAnyPressed(g_InputDB) || Console_IsOpened()) {
        ClockTimer_Sync(&m_DemoTimer);
        return false;
    }

    return ring->status == RNG_OPEN
        && ClockTimer_CheckElapsed(&m_DemoTimer, g_Config.flow.demo_delay);
}

static void M_SetupRingSwitchClose(
    INV_RING *const ring, const RING_STATUS status_target)
{
    InvRing_SetStatusTransition(
        ring, RNG_CLOSING, status_target, M_RING_SWITCH_FRAMES / 2);
}

static void M_TransitionToRing(
    INV_RING *const ring, const RING_TYPE source_type,
    const RING_TYPE target_type)
{
    g_InvRing_Source[source_type].current = ring->current_object;
    g_InvRing_Source[source_type].count = ring->number_of_objects;
    ring->type = target_type;
    ring->list = g_InvRing_Source[target_type].items;
    ring->number_of_objects = g_InvRing_Source[target_type].count;
    ring->current_object = g_InvRing_Source[target_type].current;
    InvRing_SetStatusTransition(
        ring, RNG_OPENING, RNG_OPEN, M_RING_SWITCH_FRAMES / 2);
}

static void M_SnapshotRingState(INV_RING *const ring)
{
    ring->prev_radius = ring->radius;
    ring->prev_camera_y = ring->camera.pos.y;
    ring->prev_camera_pitch = ring->camera_pitch;
    ring->prev_ring_rot_y = ring->ring_pos.rot.y;
}

static void M_SnapshotItemState(INVENTORY_ITEM *const inv_item)
{
    inv_item->prev_x_rot_pt = inv_item->x_rot_pt;
    inv_item->prev_x_rot = inv_item->x_rot;
    inv_item->prev_y_rot = inv_item->y_rot;
    inv_item->prev_y_trans = inv_item->y_trans;
    inv_item->prev_z_trans = inv_item->z_trans;
    inv_item->prev_manual_rot = inv_item->manual_rot;
}

static void M_SnapshotFrameState(INV_RING *const ring)
{
    M_SnapshotRingState(ring);
    for (int32_t i = 0; i < ring->number_of_objects; i++) {
        M_SnapshotItemState(ring->list[i]);
    }
}

static GF_COMMAND M_Control(INV_RING *const ring)
{
    if (ring->status == RNG_OPENING) {
        if (ring->mode == INV_TITLE_MODE
            && (Fader_IsActive(&ring->top_fader)
                || Fader_IsActive(&ring->back_fader))) {
            return (GF_COMMAND) { .action = GF_NOOP };
        }

        ClockTimer_Sync(&m_DemoTimer);
        if (!ring->has_spun_out) {
            Sound_Effect(SFX_MENU_SPININ, nullptr, SPM_ALWAYS);
            ring->has_spun_out = true;
        }
    }

    if (ring->status == RNG_FADING_OUT) {
        if (ring->mode == INV_TITLE_MODE) {
            const GF_COMMAND gf_cmd = M_Finish(ring, true);
            ring->is_done = true;
            ring->status = RNG_DONE;
            return gf_cmd;
        }

        if (!Fader_IsActive(&ring->back_fader)
            && !Fader_IsActive(&ring->top_fader)) {
            Fader_InitFromCurrentHold(
                &ring->top_fader, 1.0f, M_INV_RING_FADE_TIME_TO_BLACK,
                1.0f / (float)LOGIC_FPS);
        }

        if (Fader_IsActive(&ring->top_fader)
            || Fader_IsActive(&ring->back_fader)) {
            return (GF_COMMAND) { .action = GF_NOOP };
        }
        ring->status = RNG_DONE;
    }

    if (ring->status == RNG_DONE && !ring->is_done) {
        const GF_COMMAND gf_cmd = M_Finish(ring, true);
        ring->is_done = true;
        // Returning to game – resume music
        if (gf_cmd.action == GF_NOOP) {
            Music_Unpause();
            Sound_UnpauseAll();
        }
        return gf_cmd;
    }

    InvRing_CalcAdders(ring, INV_RING_ROTATE_DURATION);

    Input_Update();
    // Do the demo inactivity check prior to postprocessing of the inputs.
    if (M_CheckDemoTimer(ring)) {
        ring->is_demo_needed = true;
    }
    Shell_ProcessInput();
    Game_ProcessInput();

    if (ring->mode == INV_GLOBE_SELECT_MODE) {
        m_StartLevel = -1;
    } else {
        m_StartLevel = Game_IsLevelComplete() ? g_Passport.select_level : -1;
    }

    if (g_Config.gameplay.enable_timer_in_inventory
        && !(Game_IsInGym() && Gym_TrackManager_HasStats(GYM_TRACK_ASSAULT))) {
        Stats_UpdateTimer();
    }

    if (Shell_IsExiting()) {
        return (GF_COMMAND) { .action = GF_EXIT_GAME };
    }

    if ((ring->mode == INV_SAVE_MODE || ring->mode == INV_SAVE_CRYSTAL_MODE
         || ring->mode == INV_LOAD_MODE || ring->mode == INV_DEATH_MODE
         || ring->mode == INV_GLOBE_SELECT_MODE)
        && !ring->is_pass_open) {
        g_Input = (INPUT_STATE) {};
        g_InputDB = (INPUT_STATE) { .menu_confirm = 1 };
    }

    if (ring->mode != INV_TITLE_MODE && !Fader_IsActive(&ring->back_fader)
        && !Fader_IsActive(&ring->top_fader) && ring->status != RNG_OPENING) {
        for (int i = 0; i < ring->number_of_objects; i++) {
            INVENTORY_ITEM *const inv_item = ring->list[i];
            if (inv_item->object_id == O_COMPASS_OPTION) {
                Option_Stats_UpdateCompassNeedle(inv_item);
            }
        }
    }

    if (ring->rotating) {
        return (GF_COMMAND) { .action = GF_NOOP };
    }

    switch (ring->status) {
    case RNG_OPEN:
        if (g_Input.menu_right && ring->number_of_objects > 1) {
            InvRing_RotateLeft(ring);
            Sound_Effect(SFX_MENU_ROTATE, nullptr, SPM_ALWAYS);
            break;
        }

        if (g_Input.menu_left && ring->number_of_objects > 1) {
            InvRing_RotateRight(ring);
            Sound_Effect(SFX_MENU_ROTATE, nullptr, SPM_ALWAYS);
            break;
        }

        if (m_StartLevel != -1 || ring->is_demo_needed
            || (g_InputDB.menu_back && ring->mode != INV_TITLE_MODE
                && ring->mode != INV_GLOBE_SELECT_MODE)) {
            Sound_Effect(SFX_MENU_SPINOUT, nullptr, SPM_ALWAYS);
            m_InvChosen = NO_OBJECT;

            if (ring->type == RT_MAIN) {
                g_InvRing_Source[RT_MAIN].current = ring->current_object;
            } else if (ring->type != RT_NUMBER_OF) {
                g_InvRing_Source[ring->type].current = ring->current_object;
            }

            if (M_Finish(ring, false).action != GF_NOOP) {
                InvRing_SetStatusTransition(
                    ring, RNG_CLOSING, RNG_FADING_OUT, INV_RING_CLOSE_FRAMES);
            } else {
                InvRing_SetStatusTransition(
                    ring, RNG_CLOSING, RNG_DONE, INV_RING_CLOSE_FRAMES);
                if (g_Config.visuals.enable_fade_effects
                    && g_Config.ui.inventory_fade_effects) {
                    Fader_InitFromCurrent(
                        &ring->back_fader, 0.0f, M_INV_RING_FADE_TIME_FAST);
                }
            }

            g_Input = (INPUT_STATE) {};
            g_InputDB = (INPUT_STATE) {};
        }

        const bool examine = g_InputDB.look && InvRing_CanExamine();
        if (g_InputDB.menu_confirm || examine) {
            if ((ring->mode == INV_SAVE_MODE
                 || ring->mode == INV_SAVE_CRYSTAL_MODE
                 || ring->mode == INV_LOAD_MODE || ring->mode == INV_DEATH_MODE
                 || ring->mode == INV_GLOBE_SELECT_MODE)
                && !ring->is_pass_open) {
                ring->is_pass_open = true;
            }

            g_InvRing_Source[ring->type].current = ring->current_object;
            INVENTORY_ITEM *const inv_item =
                g_InvRing_Source[ring->type].items[ring->current_object];

            if (examine) {
                inv_item->action = ACTION_EXAMINE;
                inv_item->goal_frame = 0;
                inv_item->anim_direction = 1;
            } else {
                inv_item->action = ACTION_USE;
                inv_item->goal_frame = inv_item->open_frame;
                inv_item->anim_direction = 1;
            }
            InvRing_SetStatusTransition(
                ring, RNG_SELECTING, RNG_SELECTED, M_SELECTING_FRAMES);
            g_Input = (INPUT_STATE) {};
            g_InputDB = (INPUT_STATE) {};

            switch (inv_item->object_id) {
            case O_COMPASS_OPTION:
                Sound_Effect(SFX_MENU_COMPASS, nullptr, SPM_ALWAYS);
                break;

            case O_STOPWATCH_OPTION:
                break;

            case O_PHOTO_OPTION:
                Sound_Effect(SFX_MENU_LARA_HOME, nullptr, SPM_ALWAYS);
                break;

            case O_CONTROL_OPTION:
                Sound_Effect(SFX_MENU_GAMEBOY, nullptr, SPM_ALWAYS);
                break;

            case O_PISTOL_OPTION:
            case O_SHOTGUN_OPTION:
            case O_MAGNUM_OPTION:
            case O_AUTOS_OPTION:
            case O_DESERT_EAGLE_OPTION:
            case O_UZI_OPTION:
            case O_HARPOON_OPTION:
            case O_M16_OPTION:
            case O_MP5_OPTION:
            case O_GRENADE_GUN_OPTION:
            case O_ROCKET_GUN_OPTION:
                Sound_Effect(SFX_MENU_GUNS, nullptr, SPM_ALWAYS);
                break;

            default:
                Sound_Effect(SFX_MENU_CHOOSE, nullptr, SPM_ALWAYS);
                break;
            }
        }

        if (g_InputDB.menu_up && ring->mode != INV_TITLE_MODE
            && ring->mode != INV_KEYS_MODE
            && ring->mode != INV_GLOBE_SELECT_MODE) {
            if (ring->type == RT_MAIN) {
                if (g_InvRing_Source[RT_KEYS].count > 0) {
                    M_SetupRingSwitchClose(ring, RNG_MAIN2KEYS);
                }
                g_Input = (INPUT_STATE) {};
                g_InputDB = (INPUT_STATE) {};
            } else if (ring->type == RT_OPTION) {
                if (g_InvRing_Source[RT_MAIN].count > 0) {
                    M_SetupRingSwitchClose(ring, RNG_OPTION2MAIN);
                }
                g_InputDB = (INPUT_STATE) {};
            }
        } else if (
            g_InputDB.menu_down && ring->mode != INV_TITLE_MODE
            && ring->mode != INV_KEYS_MODE
            && ring->mode != INV_GLOBE_SELECT_MODE) {
            if (ring->type == RT_MAIN) {
                if (g_InvRing_Source[RT_OPTION].count > 0
                    && !InvRing_IsOptionLockedOut()) {
                    M_SetupRingSwitchClose(ring, RNG_MAIN2OPTION);
                }
                g_InputDB = (INPUT_STATE) {};
            } else if (ring->type == RT_KEYS) {
                if (g_InvRing_Source[RT_MAIN].count > 0) {
                    M_SetupRingSwitchClose(ring, RNG_KEYS2MAIN);
                }
                g_Input = (INPUT_STATE) {};
                g_InputDB = (INPUT_STATE) {};
            }
        }
        break;

    case RNG_MAIN2OPTION:
        M_TransitionToRing(ring, RT_MAIN, RT_OPTION);
        break;

    case RNG_MAIN2KEYS:
        M_TransitionToRing(ring, RT_MAIN, RT_KEYS);
        break;

    case RNG_KEYS2MAIN:
        M_TransitionToRing(ring, RT_KEYS, RT_MAIN);
        break;

    case RNG_OPTION2MAIN:
        M_TransitionToRing(ring, RT_OPTION, RT_MAIN);
        break;

    case RNG_SELECTED: {
        INVENTORY_ITEM *inv_item = ring->list[ring->current_object];
        if (inv_item->object_id == O_PASSPORT_CLOSED) {
            inv_item->object_id = O_PASSPORT_OPTION;
        }

        bool busy = false;
        for (int32_t frame = 0; frame < INV_RING_FRAMES; frame++) {
            busy = false;
            if (inv_item->y_rot == inv_item->y_rot_sel) {
                busy = M_AnimateInventoryItem(inv_item);
            }
        }

        Option_Control(inv_item, busy);

        if (!busy) {
            if (g_InputDB.menu_back && ring->mode != INV_GLOBE_SELECT_MODE) {
                InvRing_SetStatusTransition(
                    ring, RNG_CLOSING_ITEM, RNG_DESELECT, 0);
                g_Input = (INPUT_STATE) {};
                g_InputDB = (INPUT_STATE) {};
                if (ring->mode == INV_LOAD_MODE || ring->mode == INV_SAVE_MODE
                    || ring->mode == INV_SAVE_CRYSTAL_MODE) {
                    InvRing_SetStatusTransition(
                        ring, RNG_CLOSING_ITEM, RNG_EXITING_INVENTORY, 0);
                    g_Input = (INPUT_STATE) {};
                    g_InputDB = (INPUT_STATE) {};
                }
            }

            if (g_InputDB.menu_confirm) {
                m_InvChosen = inv_item->object_id;
                if (ring->type == RT_MAIN) {
                    g_InvRing_Source[RT_MAIN].current = ring->current_object;
                } else if (ring->type != RT_NUMBER_OF) {
                    g_InvRing_Source[ring->type].current = ring->current_object;
                }

                if (ring->mode == INV_TITLE_MODE
                    && (inv_item->object_id == O_DETAIL_OPTION
                        || inv_item->object_id == O_SOUND_OPTION
                        || inv_item->object_id == O_PDA_OPTION
                        || inv_item->object_id == O_CONTROL_OPTION
                        || inv_item->object_id == O_GLOBE_SELECT_OPTION)) {
                    InvRing_SetStatusTransition(
                        ring, RNG_CLOSING_ITEM, RNG_DESELECT, 0);
                } else {
                    InvRing_SetStatusTransition(
                        ring, RNG_CLOSING_ITEM, RNG_EXITING_INVENTORY, 0);
                }
                g_Input = (INPUT_STATE) {};
                g_InputDB = (INPUT_STATE) {};
            }
        }
        break;
    }

    case RNG_DESELECT: {
        INVENTORY_ITEM *const inv_item = ring->list[ring->current_object];
        Option_Close(inv_item);
        Sound_Effect(SFX_MENU_SPINOUT, nullptr, SPM_ALWAYS);
        InvRing_SetStatusTransition(
            ring, RNG_DESELECTING, RNG_OPEN, M_SELECTING_FRAMES);
        g_Input = (INPUT_STATE) {};
        g_InputDB = (INPUT_STATE) {};
        break;
    }

    case RNG_CLOSING_ITEM: {
        INVENTORY_ITEM *inv_item = ring->list[ring->current_object];
        for (int32_t frame = 0; frame < INV_RING_FRAMES; frame++) {
            if (!M_AnimateInventoryItem(inv_item)) {
                if (inv_item->object_id == O_PASSPORT_OPTION) {
                    inv_item->object_id = O_PASSPORT_CLOSED;
                    inv_item->current_frame = 0;
                }

                InvRing_SetStatusTransition(
                    ring, ring->status_target, ring->status_target,
                    M_SELECTING_FRAMES);
                break;
            }
        }
        break;
    }

    case RNG_EXITING_INVENTORY:
        if (ring->status_frames == 0) {
            if (M_Finish(ring, false).action != GF_NOOP) {
                // Fade to black. Do it later once reaching RNG_FADING_OUT.
                InvRing_SetStatusTransition(
                    ring, RNG_CLOSING, RNG_FADING_OUT, INV_RING_CLOSE_FRAMES);
            } else {
                // Fade to game. Do it as soon as the ring starts to close.
                InvRing_SetStatusTransition(
                    ring, RNG_CLOSING, RNG_DONE, INV_RING_CLOSE_FRAMES);
                if (g_Config.visuals.enable_fade_effects
                    && g_Config.ui.inventory_fade_effects) {
                    Fader_InitFromCurrent(
                        &ring->back_fader, 0.0f, M_INV_RING_FADE_TIME_FAST);
                }
            }
        }
        break;

    default:
        break;
    }

    if (ring->status == RNG_OPEN || ring->status == RNG_SELECTING
        || ring->status == RNG_SELECTED || ring->status == RNG_DESELECTING
        || ring->status == RNG_DESELECT || ring->status == RNG_CLOSING_ITEM) {
        if (!ring->rotating
            && ((!g_Input.menu_left && !g_Input.menu_right)
                || ring->number_of_objects <= 1)) {
            INVENTORY_ITEM *const inv_item = ring->list[ring->current_object];
            M_RingNotActive(ring, inv_item);
        }
        M_RingIsOpen(ring);
    } else {
        M_RingIsNotOpen(ring);
    }

    if (ring->status == RNG_OPENING || ring->status == RNG_CLOSING
        || ring->status == RNG_MAIN2OPTION || ring->status == RNG_OPTION2MAIN
        || ring->status == RNG_EXITING_INVENTORY
        || ring->status == RNG_FADING_OUT || ring->status == RNG_DONE
        || ring->rotating) {
        M_RingActive();
    }

    Interpolation_Remember();
    return (GF_COMMAND) { .action = GF_NOOP };
}

void InvRing_RemoveAllText(void)
{
    InvRing_RemoveHeader();
    InvRing_RemoveItemTexts();
    InvRing_ClearButtonHint();
}

INV_RING *InvRing_Open(const INVENTORY_MODE mode)
{
    if (mode == INV_KEYS_MODE && g_InvRing_Source[RT_KEYS].count == 0) {
        m_InvChosen = NO_OBJECT;
        return nullptr;
    }

    m_InvChosen = NO_OBJECT;

    g_InvRing_OldCamera = g_Camera;
    m_StartLevel = -1;

    if (mode == INV_TITLE_MODE) {
        InvRing_ShowVersionText();
        Savegame_ScanSavedGames();
    } else {
        InvRing_RemoveVersionText();
    }

    if (mode != INV_GLOBE_SELECT_MODE) {
        // Reset option ring
        g_InvRing_Source[RT_OPTION].count = 0;
        Inv_InsertItem(
            InvRing_GetByObjectID(O_PASSPORT_CLOSED) != nullptr
                ? InvRing_GetByObjectID(O_PASSPORT_CLOSED)
                : InvRing_GetByObjectID(O_PASSPORT_OPTION));
        if (g_TRVersion == 1) {
            Inv_InsertItem(InvRing_GetByObjectID(O_CONTROL_OPTION));
            Inv_InsertItem(InvRing_GetByObjectID(O_SOUND_OPTION));
            Inv_InsertItem(InvRing_GetByObjectID(O_DETAIL_OPTION));
        } else {
            Inv_InsertItem(InvRing_GetByObjectID(O_DETAIL_OPTION));
            Inv_InsertItem(InvRing_GetByObjectID(O_CONTROL_OPTION));
            Inv_InsertItem(InvRing_GetByObjectID(O_SOUND_OPTION));
        }
        Inv_InsertItem(InvRing_GetByObjectID(O_PDA_OPTION));
        if (mode == INV_TITLE_MODE && GF_GetGymLevel() != nullptr) {
            Inv_InsertItem(InvRing_GetByObjectID(O_PHOTO_OPTION));
        }
    } else if (g_InvRing_Source[RT_GLOBE_SELECT].count == 0) {
        INVENTORY_ITEM *const globe =
            InvRing_GetByObjectID(O_GLOBE_SELECT_OPTION);
        if (globe != nullptr) {
            Inv_InsertItem(globe);
        }
    }

    g_InvRing_Source[RT_KEYS].current = 0;
    for (int32_t i = 0; i < g_InvRing_Source[RT_KEYS].count; i++) {
        InvRing_InitInvItem(g_InvRing_Source[RT_KEYS].items[i]);
    }

    g_InvRing_Source[RT_MAIN].current = 0;
    for (int32_t i = 0; i < g_InvRing_Source[RT_MAIN].count; i++) {
        InvRing_InitInvItem(g_InvRing_Source[RT_MAIN].items[i]);
    }

    g_InvRing_Source[RT_OPTION].current = 0;
    for (int32_t i = 0; i < g_InvRing_Source[RT_OPTION].count; i++) {
        g_InvRing_Source[RT_OPTION].qtys[i] = 1;
        InvRing_InitInvItem(g_InvRing_Source[RT_OPTION].items[i]);
    }

    g_InvRing_Source[RT_GLOBE_SELECT].current = 0;
    for (int32_t i = 0; i < g_InvRing_Source[RT_GLOBE_SELECT].count; i++) {
        g_InvRing_Source[RT_GLOBE_SELECT].qtys[i] = 1;
        InvRing_InitInvItem(g_InvRing_Source[RT_GLOBE_SELECT].items[i]);
    }

    if (mode == INV_TITLE_MODE && GF_GetGymLevel() != nullptr
        && Gym_IsInventoryOpenEnabled()) {
        for (int32_t i = 0; i < g_InvRing_Source[RT_OPTION].count; i++) {
            if (g_InvRing_Source[RT_OPTION].items[i]->object_id
                == O_PHOTO_OPTION) {
                g_InvRing_Source[RT_OPTION].current = i;
            }
        }
    }

    if (!g_Config.audio.enable_music_in_inventory && mode != INV_TITLE_MODE) {
        Music_Pause();
        Sound_PauseAll();
    }

    INV_RING *const ring = Memory_Alloc(sizeof(INV_RING));
    ring->mode = mode;
    ring->background_style = mode == INV_TITLE_MODE
        ? BK_IMAGE
        : g_Config.ui.inventory_background_style;
    ring->background_path = ring->background_style == BK_IMAGE
        ? g_GameFlow.main_menu_background_path
        : nullptr;

    switch (mode) {
    case INV_GLOBE_SELECT_MODE:
        ring->background_style = BK_NONE;
        ring->background_path = nullptr;
        InvRing_InitRing(
            ring, RT_GLOBE_SELECT, g_InvRing_Source[RT_GLOBE_SELECT].items,
            g_InvRing_Source[RT_GLOBE_SELECT].count,
            g_InvRing_Source[RT_GLOBE_SELECT].current);
        Option_GlobeSelect_UpdateSelectable(ring);
        break;

    case INV_TITLE_MODE:
    case INV_SAVE_MODE:
    case INV_SAVE_CRYSTAL_MODE:
    case INV_LOAD_MODE:
    case INV_DEATH_MODE:
        InvRing_InitRing(
            ring, RT_OPTION, g_InvRing_Source[RT_OPTION].items,
            g_InvRing_Source[RT_OPTION].count,
            g_InvRing_Source[RT_OPTION].current);
        break;

    case INV_KEYS_MODE:
        InvRing_InitRing(
            ring, RT_KEYS, g_InvRing_Source[RT_KEYS].items,
            g_InvRing_Source[RT_KEYS].count, g_InvRing_Source[RT_MAIN].current);
        break;

    default:
        if (g_InvRing_Source[RT_MAIN].count > 0) {
            InvRing_InitRing(
                ring, RT_MAIN, g_InvRing_Source[RT_MAIN].items,
                g_InvRing_Source[RT_MAIN].count,
                g_InvRing_Source[RT_MAIN].current);
        } else {
            InvRing_InitRing(
                ring, RT_OPTION, g_InvRing_Source[RT_OPTION].items,
                g_InvRing_Source[RT_OPTION].count,
                g_InvRing_Source[RT_OPTION].current);
        }
        break;
    }

    g_Inv_Mode = mode;
    Interpolation_Remember();

    if (mode == INV_TITLE_MODE) {
        if (ring->background_path != nullptr) {
            Output_Overlay_LoadImage(ring->background_path);
        }
        Fader_InitTo(&ring->top_fader, 1.0f, 0.0f, M_INV_RING_FADE_TIME_FAST);
    } else {
        Fader_InitTo(&ring->back_fader, 0.0f, 1.0f, M_INV_RING_FADE_TIME_FAST);
    }

    return ring;
}

void InvRing_Close(INV_RING *const ring)
{
    InvRing_RemoveAllText();
    InvRing_RemoveVersionText();

    if (ring->list != nullptr) {
        INVENTORY_ITEM *const inv_item = ring->list[ring->current_object];
        if (inv_item != nullptr) {
            Option_Close(inv_item);
        }
    }
    if (ring->mode == INV_TITLE_MODE) {
        Music_Stop();
        Sound_StopAll();
    }

    if (g_Config.input.enable_buffering_inventory) {
        g_OldInputDB = (INPUT_STATE) {};
    }

    m_InvChosen = NO_OBJECT;
    Memory_Free(ring);
}

GF_COMMAND InvRing_Control(INV_RING *const ring)
{
    InvRing_AdjustMusicVolume(ring);
    m_ActiveRing = ring;
    INVENTORY_ITEM **const prev_list = ring->list;
    M_SnapshotFrameState(ring);
    GF_COMMAND gf_cmd = M_Control(ring);

    if (ring->status != RNG_OPENING && ring->status != RNG_DONE
        && ring->status != RNG_FADING_OUT) {
        for (int32_t frame = 0; frame < INV_RING_FRAMES; frame++) {
            for (int32_t i = 0; i < ring->number_of_objects; i++) {
                InvRing_UpdateInventoryItem(ring, ring->list[i]);
            }
        }
    }

    if (ring->status != RNG_DONE
        && (ring->status != RNG_OPENING
            || (ring->mode != INV_TITLE_MODE
                || (!Fader_IsActive(&ring->top_fader)
                    && !Fader_IsActive(&ring->back_fader))))) {
        for (int32_t frame = 0; frame < INV_RING_FRAMES; frame++) {
            InvRing_DoMotions(ring);
        }
    }

    if (ring->list != prev_list) {
        M_SnapshotFrameState(ring);
    }

    // Running motions in control can reach RNG_DONE in this same tick.
    // Finalize immediately so phase code receives the non-NOOP GF command.
    if (gf_cmd.action == GF_NOOP && ring->status == RNG_DONE
        && !ring->is_done) {
        gf_cmd = M_Control(ring);
    }

    m_ActiveRing = nullptr;
    Overlay_Animate(1);
    return gf_cmd;
}

bool InvRing_IsRingAvailable(const RING_TYPE ring_type)
{
    if (ring_type == RT_OPTION && InvRing_IsOptionLockedOut()) {
        return false;
    }
    return g_InvRing_Source[ring_type].count > 0;
}

bool InvRing_IsOptionLockedOut(void)
{
    return g_Config.flow.lockout_option_ring;
}

INVENTORY_ITEM *InvRing_GetByObjectID(const OBJECT_ID object_id)
{
    for (int32_t i = 0; i < g_InvRing_Items->count; i++) {
        INVENTORY_ITEM *const item =
            *(INVENTORY_ITEM **)Vector_Get(g_InvRing_Items, i);
        if (item->object_id == object_id) {
            return item;
        }
    }
    return nullptr;
}
