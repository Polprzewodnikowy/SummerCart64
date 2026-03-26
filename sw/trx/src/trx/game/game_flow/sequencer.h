#pragma once

#include <trx/game/game_flow/types.h>
#include <trx/game/inventory_ring/types.h>
#include <trx/game/savegame/types.h>

GF_COMMAND GF_EnterPhotoMode(void);
GF_COMMAND GF_PauseGame(void);
GF_COMMAND GF_ShowInventory(INVENTORY_MODE inv_mode);
bool GF_ShowInventoryKeys(OBJECT_ID receptacle_type_id);
GF_COMMAND GF_RunTitle(void);
GF_COMMAND GF_RunDemo(int32_t demo_num);
GF_COMMAND GF_RunCutscene(int32_t cutscene_num);
GF_COMMAND GF_RunGame(const GF_LEVEL *level, GF_SEQUENCE_CONTEXT seq_ctx);
GF_COMMAND GF_RunGlobeSelect(const char *background_path);

GF_COMMAND GF_DoFrontendSequence(void);
GF_COMMAND GF_DoDemoSequence(int32_t demo_num);
GF_COMMAND GF_DoCutsceneSequence(int32_t cutscene_num);

GF_COMMAND GF_InterpretSequence(
    const GF_LEVEL *level, GF_SEQUENCE_CONTEXT seq_ctx, void *seq_ctx_arg);

GF_COMMAND GF_DoLevelSequence(
    const GF_LEVEL *start_level, GF_SEQUENCE_CONTEXT seq_ctx);

GF_COMMAND GF_PlayAvailableStory(SAVEGAME_SLOT_REF slot);
