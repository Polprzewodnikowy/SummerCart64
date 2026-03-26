#include <trx/config.h>
#include <trx/core/log.h>
#include <trx/core/memory.h>
#include <trx/game/demo.h>
#include <trx/game/game.h>
#include <trx/game/game_flow/common.h>
#include <trx/game/game_flow/sequencer.h>
#include <trx/game/game_flow/vars.h>
#include <trx/game/game_strings/table.h>
#include <trx/game/inventory.h>
#include <trx/game/inventory_ring/control.h>
#include <trx/game/level.h>
#include <trx/game/objects/vars.h>
#include <trx/game/phase.h>
#include <trx/game/savegame.h>
#include <trx/game/shell/common.h>

GF_COMMAND GF_RunTitle(void)
{
    Savegame_UnbindSlot();
    GameStringTable_Apply(nullptr);
    const GF_LEVEL *const title_level = GF_GetTitleLevel();
    if (!Level_Initialise(title_level, GFSC_NORMAL)) {
        return (GF_COMMAND) { .action = GF_EXIT_GAME };
    }
    return GF_ShowInventory(INV_TITLE_MODE);
}

GF_COMMAND GF_EnterPhotoMode(void)
{
    PHASE *const subphase = Phase_PhotoMode_Create();
    const GF_COMMAND gf_cmd = PhaseExecutor_Run(subphase);
    Phase_PhotoMode_Destroy(subphase);
    return gf_cmd;
}

GF_COMMAND GF_PauseGame(void)
{
    PHASE *const subphase = Phase_Pause_Create();
    const GF_COMMAND gf_cmd = PhaseExecutor_Run(subphase);
    Phase_Pause_Destroy(subphase);
    return gf_cmd;
}

GF_COMMAND GF_ShowInventory(const INVENTORY_MODE mode)
{
    PHASE *const phase = Phase_Inventory_Create(mode);
    const GF_COMMAND gf_cmd = PhaseExecutor_Run(phase);
    Phase_Inventory_Destroy(phase);
    return gf_cmd;
}

bool GF_ShowInventoryKeys(const OBJECT_ID receptacle_type_id)
{
    if (!InvRing_IsRingAvailable(RT_KEYS)) {
        return false;
    }
    if (g_Config.gameplay.enable_auto_item_selection) {
        const OBJECT_ID obj_id = Object_GetCognateInverse(
            receptacle_type_id, g_KeyItemToReceptacleMap);
        InvRing_SetRequestedObjectID(obj_id);
    } else {
        Inv_ClearSelection();
    }
    const GF_COMMAND gf_cmd = GF_ShowInventory(INV_KEYS_MODE);
    if (gf_cmd.action != GF_NOOP) {
        GF_OverrideCommand(gf_cmd);
    }
    return true;
}

GF_COMMAND GF_RunDemo(const int32_t demo_num)
{
    PHASE *const demo_phase = Phase_Demo_Create(demo_num);
    const GF_COMMAND gf_cmd = PhaseExecutor_Run(demo_phase);
    Phase_Demo_Destroy(demo_phase);
    return gf_cmd;
}

GF_COMMAND GF_RunCutscene(const int32_t cutscene_num)
{
    PHASE *const cutscene_phase = Phase_Cutscene_Create(cutscene_num);
    const GF_COMMAND gf_cmd = PhaseExecutor_Run(cutscene_phase);
    Phase_Cutscene_Destroy(cutscene_phase);
    return gf_cmd;
}

GF_COMMAND GF_RunGame(
    const GF_LEVEL *const level, const GF_SEQUENCE_CONTEXT seq_ctx)
{
    PHASE *const phase = Phase_Game_Create(level, seq_ctx);
    const GF_COMMAND gf_cmd = PhaseExecutor_Run(phase);
    Phase_Game_Destroy(phase);
    return gf_cmd;
}

GF_COMMAND GF_RunGlobeSelect(const char *const background_path)
{
    PHASE *const phase = Phase_GlobeSelect_Create((PHASE_GLOBE_SELECT_ARGS) {
        .background_path = background_path,
    });
    const GF_COMMAND gf_cmd = PhaseExecutor_Run(phase);
    Phase_GlobeSelect_Destroy(phase);
    return gf_cmd;
}

GF_COMMAND GF_DoFrontendSequence(void)
{
    const SHELL_ARGS *const args = Shell_GetArgs();
    if (args != nullptr) {
        if (args->save_to_load >= 0) {
            return (GF_COMMAND) {
                .action = GF_START_SAVED_GAME,
                .param = Savegame_SlotToParam(
                    Savegame_NormalSlot(args->save_to_load)),
            };
        }

        if (args->level_to_select >= 0) {
            const GF_LEVEL *const level =
                GF_GetLevelByOrdinalNumber(GFLT_MAIN, args->level_to_select);
            if (level == nullptr) {
                Shell_ExitSystemFmt(
                    "Invalid level number: %d", args->level_to_select);
            }
            return (GF_COMMAND) {
                .action = GF_SELECT_GAME,
                .param = level->num,
            };
        }

        if (args->level_to_play != nullptr) {
            return (GF_COMMAND) {
                .action = GF_START_GAME,
                .param = 0,
            };
        }
    }

    if (g_GameFlow.title_level == nullptr) {
        return (GF_COMMAND) { .action = GF_NOOP };
    }
    return GF_InterpretSequence(g_GameFlow.title_level, GFSC_NORMAL, nullptr);
}

GF_COMMAND GF_DoLevelSequence(
    const GF_LEVEL *const start_level, const GF_SEQUENCE_CONTEXT seq_ctx)
{
    const GF_LEVEL *current_level = start_level;
    const GF_LEVEL_TABLE_TYPE level_table_type =
        GF_GetLevelTableType(current_level->type);
    const int32_t level_count = GF_GetLevelTable(level_table_type)->count;
    while (true) {
        const GF_COMMAND gf_cmd =
            GF_InterpretSequence(current_level, seq_ctx, nullptr);

        if (gf_cmd.action != GF_NOOP && gf_cmd.action != GF_LEVEL_COMPLETE) {
            return gf_cmd;
        }
        if (Game_IsInGym()) {
            return (GF_COMMAND) { .action = GF_EXIT_TO_TITLE };
        }
        if (current_level->num + 1 >= level_count) {
            return (GF_COMMAND) { .action = GF_EXIT_TO_TITLE };
        }
        current_level++;
    }
}

GF_COMMAND GF_DoDemoSequence(int32_t demo_num)
{
    demo_num = Demo_ChooseLevel(demo_num);
    if (demo_num < 0) {
        return (GF_COMMAND) { .action = GF_NOOP };
    }
    const GF_LEVEL *const level = GF_GetLevel(GFLT_DEMOS, demo_num);
    if (level == nullptr) {
        LOG_ERROR("Missing demo: %d", demo_num);
        return (GF_COMMAND) { .action = GF_NOOP };
    }
    return GF_InterpretSequence(level, GFSC_NORMAL, nullptr);
}

GF_COMMAND GF_DoCutsceneSequence(const int32_t cutscene_num)
{
    const GF_LEVEL *const level = GF_GetLevel(GFLT_CUTSCENES, cutscene_num);
    if (level == nullptr) {
        LOG_ERROR("Missing cutscene: %d", cutscene_num);
        return (GF_COMMAND) { .action = GF_NOOP };
    }
    return GF_InterpretSequence(level, GFSC_NORMAL, nullptr);
}

GF_COMMAND GF_PlayAvailableStory(const SAVEGAME_SLOT_REF slot)
{
    const int32_t savegame_level = Savegame_GetLevelNumber(slot);
    const bool prev_enable_legal = g_Config.gameplay.enable_legal;
    g_Config.gameplay.enable_legal = false;

    // Play intro FMVs and cutscenes
    GF_DoFrontendSequence();

    const GF_LEVEL_TABLE *const level_table = GF_GetLevelTable(GFLT_MAIN);
    for (int32_t i = 0; i <= MIN(savegame_level, level_table->count); i++) {
        const GF_LEVEL *const level = GF_GetLevel(GFLT_MAIN, i);
        if (level->type == GFL_GYM) {
            continue;
        }
        const GF_COMMAND gf_cmd = GF_InterpretSequence(
            level, GFSC_STORY, (void *)(intptr_t)savegame_level);
        if (gf_cmd.action == GF_EXIT_TO_TITLE
            || gf_cmd.action == GF_EXIT_GAME) {
            break;
        }
    }

    g_Config.gameplay.enable_legal = prev_enable_legal;
    return (GF_COMMAND) { .action = GF_EXIT_TO_TITLE };
}

bool GF_HasAvailableStory(const SAVEGAME_SLOT_REF slot)
{
    if (Savegame_IsSlotFree(slot)) {
        return false;
    }
    const int32_t savegame_level = Savegame_GetLevelNumber(slot);

    // Check intro FMVs and cutscenes in frontend sequence (skip legal FMVs)
    const GF_LEVEL *const title_level = GF_GetTitleLevel();
    if (title_level != nullptr) {
        const GF_SEQUENCE *const seq = &title_level->sequence;
        for (int32_t j = 0; j < seq->length; j++) {
            const GF_SEQUENCE_EVENT *const ev = &seq->events[j];
            if (ev->type == GFS_PLAY_CUTSCENE) {
                return true;
            }
            if (ev->type == GFS_PLAY_FMV) {
                const int32_t fmv_id = (int32_t)(intptr_t)ev->data;
                const GF_FMV *const fmv = &g_GameFlow.fmvs[fmv_id];
                if (!fmv->is_legal && !fmv->is_credit) {
                    return true;
                }
            }
        }
    }

    // Check for any cutscenes or FMVs up until the save point
    const GF_LEVEL_TABLE *const level_table = GF_GetLevelTable(GFLT_MAIN);
    const int32_t max_level = MIN(savegame_level, level_table->count);
    for (int32_t i = 0; i <= max_level; i++) {
        const GF_LEVEL *const level = GF_GetLevel(GFLT_MAIN, i);
        if (level->type == GFL_GYM) {
            continue;
        }
        const GF_SEQUENCE *const seq = &level->sequence;
        for (int32_t j = 0; j < seq->length; j++) {
            const GF_SEQUENCE_EVENT *const ev = &seq->events[j];
            // Stop checking after the saved level
            if (ev->type == GFS_LOOP_GAME) {
                break;
            }
            if (ev->type == GFS_PLAY_CUTSCENE) {
                return true;
            }
            if (ev->type == GFS_PLAY_FMV) {
                const int32_t fmv_id = (int32_t)(intptr_t)ev->data;
                const GF_FMV *const fmv = &g_GameFlow.fmvs[fmv_id];
                if (!fmv->is_legal && !fmv->is_credit) {
                    return true;
                }
            }
        }
    }
    return false;
}
