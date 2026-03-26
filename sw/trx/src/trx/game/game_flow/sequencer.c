#include <trx/game/game_flow/sequencer.h>

#include <trx/core/enum_map.h>
#include <trx/debug.h>
#include <trx/game/game.h>
#include <trx/game/game_flow/sequencer_events.h>
#include <trx/game/inventory.h>
#include <trx/game/lara/common.h>
#include <trx/game/level.h>
#include <trx/game/lua.h>
#include <trx/game/output.h>
#include <trx/game/rooms.h>
#include <trx/game/savegame.h>

static GF_COMMAND M_RunEvent(
    const GF_LEVEL *const level, const GF_SEQUENCE *const sequence,
    const int32_t event_idx, const GF_SEQUENCE_CONTEXT seq_ctx,
    void *const seq_ctx_arg)
{
    GF_COMMAND gf_cmd = { .action = GF_NOOP };
    const GF_SEQUENCE_EVENT *const event = &sequence->events[event_idx];
    LOG_DEBUG(
        "event type=%s(%d) data=0x%x",
        ENUM_MAP_TO_STRING(GF_SEQUENCE_EVENT_TYPE, event->type), event->type,
        event->data);

    const GF_SEQUENCE_EVENT_HANDLER event_handler =
        GF_GetSequenceEventHandler(event->type);
    if (event_handler == nullptr) {
        return gf_cmd;
    }

    gf_cmd = event_handler(level, sequence, event_idx, seq_ctx, seq_ctx_arg);
    LOG_DEBUG(
        "event type=%s(%d) data=0x%x finished, result: action=%s, "
        "param=%d",
        ENUM_MAP_TO_STRING(GF_SEQUENCE_EVENT_TYPE, event->type), event->type,
        event->data, ENUM_MAP_TO_STRING(GF_ACTION, gf_cmd.action),
        gf_cmd.param);
    return gf_cmd;
}

static void M_PreSequenceHook(
    const GF_SEQUENCE_CONTEXT seq_ctx, void *const seq_ctx_arg)
{
    Room_SetAbyssHeight(0);
    Output_SetSunsetEnabled(false);
    Lara_SetControllable(false);
    Lara_SetStartAnimState(LS_EXTRA_BREATH);
    if (seq_ctx == GFSC_SAVED) {
        Game_SetBonusFlag(GBF_NONE);
    }
}

static GF_SEQUENCE_CONTEXT M_SwitchSequenceContext(
    const GF_SEQUENCE_EVENT *const event, const GF_SEQUENCE_CONTEXT seq_ctx)
{
    // Update sequence context if necessary
    if (event->type != GFS_LOOP_GAME) {
        return seq_ctx;
    }
    switch (seq_ctx) {
    case GFSC_SAVED:
    case GFSC_RESTART:
    case GFSC_SELECT:
        return GFSC_NORMAL;
    default:
        return seq_ctx;
    }
}

static const GF_LEVEL *M_GetCanonicalNextLevel(const GF_LEVEL *const level)
{
    // Canonical order is still used for console-driven linear simulation.
    return GF_GetLevelAfter(level);
}

static const GF_LEVEL *M_GetLinkedPrevLevel(const GF_LEVEL *const level)
{
    RESUME_INFO *const resume = Savegame_GetCurrentInfo(level);
    if (resume == nullptr) {
        return nullptr;
    }
    if (resume->prev_level == -1) {
        return nullptr;
    }
    return GF_GetLevel(GFLT_MAIN, resume->prev_level);
}

static bool M_IsLevelDescendantOf(
    const GF_LEVEL *const level, const int32_t ancestor_level_num)
{
    RESUME_INFO *const resume = Savegame_GetCurrentInfo(level);
    if (resume == nullptr) {
        return false;
    }

    const int32_t count = GF_GetLevelTable(GFLT_MAIN)->count;
    int32_t current_prev = resume->prev_level;
    for (int32_t i = 0; i < count && current_prev != -1; i++) {
        if (current_prev == ancestor_level_num) {
            return true;
        }
        const GF_LEVEL *const prev_level = GF_GetLevel(GFLT_MAIN, current_prev);
        if (prev_level == nullptr) {
            break;
        }
        RESUME_INFO *const prev_resume = Savegame_GetCurrentInfo(prev_level);
        if (prev_resume == nullptr) {
            break;
        }
        current_prev = prev_resume->prev_level;
    }
    return false;
}

GF_COMMAND GF_InterpretSequence(
    const GF_LEVEL *const level, GF_SEQUENCE_CONTEXT seq_ctx,
    void *const seq_ctx_arg)
{
    ASSERT(level != nullptr);
    LOG_DEBUG(
        "running sequence for level=%d type=%d seq_ctx=%d", level->num,
        level->type, seq_ctx);

    if (level->type == GFL_DUMMY || level->type == GFL_CURRENT) {
        return (GF_COMMAND) { .action = GF_NOOP };
    }

    M_PreSequenceHook(seq_ctx, seq_ctx_arg);

    GF_COMMAND gf_cmd = { .action = GF_EXIT_TO_TITLE };

    const GF_LEVEL *const prev_level = M_GetLinkedPrevLevel(level);

    // before load
    switch (seq_ctx) {
    case GFSC_STORY:
        break;

    case GFSC_SAVED:
        GF_InventoryModifier_Scan(level);
        // reset current info to the defaults so that we do not do
        // Item_GlobalReplace in the inventory initialization routines too early
        Savegame_InitCurrentInfo();
        break;

    case GFSC_RESTART:
        if (level == GF_GetGymLevel() || level == GF_GetFirstLevel()) {
            Savegame_InitCurrentInfo();
        } else {
            const int32_t prev_level_num =
                Savegame_GetCurrentInfo(level)->prev_level;
            Savegame_ResetCurrentInfo(level);
            if (prev_level_num != -1) {
                const GF_LEVEL *const linked_prev_level =
                    GF_GetLevel(GFLT_MAIN, prev_level_num);
                if (linked_prev_level != nullptr) {
                    Savegame_CarryCurrentInfoToNextLevel(
                        linked_prev_level, level);
                }
            }
            Savegame_ApplyLogicToCurrentInfo(level);
        }
        if (level->type == GFL_NORMAL || level->type == GFL_BONUS) {
            GF_InventoryModifier_Scan(level);
            GF_InventoryModifier_ApplyToResumeInfo(level);
        }
        break;

    case GFSC_SELECT: {
        const SAVEGAME_SLOT_REF slot = Savegame_GetBoundSlot();
        if (Savegame_IsValidSlotRef(slot)) {
            // select level feature
            Savegame_InitCurrentInfo();
            if (level->num > GF_GetFirstLevel()->num) {
                Savegame_LoadOnlyResumeInfo(slot);

                const int32_t prev_level_num =
                    Savegame_GetCurrentInfo(level)->prev_level;

                const GF_LEVEL_TABLE *const level_table =
                    GF_GetLevelTable(GFLT_MAIN);
                for (int32_t i = 0; i < level_table->count; i++) {
                    const GF_LEVEL *const tmp_level = &level_table->levels[i];
                    if (tmp_level->type == GFL_GYM) {
                        continue;
                    }
                    if (tmp_level == level
                        || M_IsLevelDescendantOf(tmp_level, level->num)) {
                        Savegame_ResetCurrentInfo(tmp_level);
                    }
                }

                if (prev_level_num != -1) {
                    const GF_LEVEL *const linked_prev_level =
                        GF_GetLevel(GFLT_MAIN, prev_level_num);
                    if (linked_prev_level != nullptr) {
                        Savegame_CarryCurrentInfoToNextLevel(
                            linked_prev_level, level);
                    }
                }

                Savegame_ApplyLogicToCurrentInfo(level);
                GF_InventoryModifier_Scan(level);
                GF_InventoryModifier_ApplyToResumeInfo(level);
            } else {
                Savegame_ApplyLogicToCurrentInfo(level);
                GF_InventoryModifier_Scan(level);
                GF_InventoryModifier_ApplyToResumeInfo(level);
            }
        } else {
            // console /play level feature
            Inv_RemoveAllItems();
            if (level == GF_GetGymLevel()) {
                Savegame_InitCurrentInfo();
                GF_InventoryModifier_Scan(level);
                GF_InventoryModifier_ApplyToResumeInfo(level);
            } else {
                const GF_LEVEL *tmp_level = GF_GetFirstLevel();
                Savegame_ResetCurrentInfo(tmp_level);
                while (tmp_level != nullptr && tmp_level <= level) {
                    Savegame_ApplyLogicToCurrentInfo(tmp_level);
                    GF_InventoryModifier_Scan(tmp_level);
                    GF_InventoryModifier_ApplyToResumeInfo(tmp_level);
                    if (tmp_level == level) {
                        break;
                    }

                    const GF_LEVEL *const next_level =
                        M_GetCanonicalNextLevel(tmp_level);
                    if (next_level != nullptr) {
                        Savegame_CarryCurrentInfoToNextLevel(
                            tmp_level, next_level);
                    }
                    tmp_level = next_level;
                }
            }
        }
        break;
    }

    default:
        if (level->type == GFL_GYM) {
            Savegame_ResetCurrentInfo(level);
            Savegame_ApplyLogicToCurrentInfo(level);
        } else if (level->type == GFL_DEMO) {
            Savegame_ApplyLogicToCurrentInfo(level);
        } else if (level->type == GFL_NORMAL || level->type == GFL_BONUS) {
            Savegame_ApplyLogicToCurrentInfo(level);
            GF_InventoryModifier_Scan(level);
            GF_InventoryModifier_ApplyToResumeInfo(level);
        }
    }

    // Run any level Lua script
    Lua_ClearLevelListeners();
    Lua_SetScriptContext(LUA_CONTEXT_LEVEL);
    if (level->script_path != nullptr) {
        LUA_RESULT res = Lua_EvalFile(level->script_path);
        if (res.code != LUA_OK) {
            LOG_ERROR("Lua level script error: %s", res.message);
        }
        Lua_FreeResult(&res);
    }
    Lua_SetScriptContext(LUA_CONTEXT_GLOBAL);

    // load the level
    if (seq_ctx != GFSC_STORY || level->type == GFL_CUTSCENE) {
        if (!Level_Initialise(level, seq_ctx)) {
            Game_SetCurrentLevel(nullptr);
            GF_SetCurrentLevel(nullptr);
            if (level->type == GFL_TITLE) {
                gf_cmd = (GF_COMMAND) { .action = GF_EXIT_GAME };
            } else {
                gf_cmd = (GF_COMMAND) { .action = GF_EXIT_TO_TITLE };
            }
        }
    }

    const GF_SEQUENCE *const sequence = &level->sequence;
    for (int32_t i = 0; i < sequence->length; i++) {
        const GF_SEQUENCE_EVENT *const event = &sequence->events[i];
        gf_cmd = M_RunEvent(level, sequence, i, seq_ctx, seq_ctx_arg);
        if (gf_cmd.action != GF_NOOP) {
            return gf_cmd;
        }

        // Update sequence context if necessary
        seq_ctx = M_SwitchSequenceContext(event, seq_ctx);
    }

    LOG_DEBUG(
        "sequence finished: action=%s param=%d",
        ENUM_MAP_TO_STRING(GF_ACTION, gf_cmd.action), gf_cmd.param);
    return gf_cmd;
}
