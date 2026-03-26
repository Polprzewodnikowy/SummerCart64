#include <trx/core/memory.h>
#include <trx/game/anims.h>
#include <trx/game/level/finalize.h>

void Level_Finalize_LoadAnimCommands(LEVEL_CONTEXT *const ctx)
{
    LEVEL_CONTEXT_INFO *const info = &ctx->info;
    Anim_LoadCommands(info->anims.commands);
    Memory_FreePointer(&info->anims.commands);
}

void Level_Finalize_LoadAnimFrames(LEVEL_CONTEXT *const ctx)
{
    const LEVEL_FORMAT_LOADER *const loader = ctx->loader;
    LEVEL_CONTEXT_INFO *const info = &ctx->info;
    const int32_t frame_count =
        Anim_GetTotalFrameCount(loader, info->anims.frame_count);
    Anim_InitialiseFrames(frame_count);
    Anim_LoadFrames(loader, info->anims.frames, info->anims.frame_count);
    Memory_FreePointer(&info->anims.frames);
}
