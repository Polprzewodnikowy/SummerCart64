#include <trx/core/strings.h>
#include <trx/game/console/common.h>
#include <trx/game/console/registry.h>
#include <trx/game/game_strings/entries.h>
#include <trx/game/music.h>

static COMMAND_RESULT M_Entrypoint(const COMMAND_CONTEXT *const ctx)
{
    int32_t track_to_play = -1;
    if (String_IsEmpty(ctx->args)) {
        return CR_BAD_INVOCATION;
    } else if (String_ParseInteger(ctx->args, &track_to_play)) {
        if (track_to_play == 0 || track_to_play == -1) {
            Music_Stop();
            Console_Log(GS("console/cmd/play_music/stopped"));
        } else if (Music_Play_Direct(track_to_play, MPM_ONCE)) {
            Console_Log(GS("console/cmd/play_music/track"), track_to_play);
        } else {
            Console_LogError(GS("console/cmd/play_music/invalid_track"));
        }
        return CR_SUCCESS;
    } else {
        return CR_BAD_INVOCATION;
    }
}

REGISTER_CONSOLE_COMMAND("music", M_Entrypoint, GS_ID("console/cmd/music/help"))
