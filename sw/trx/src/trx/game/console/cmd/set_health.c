#include <trx/core/strings.h>
#include <trx/core/utils.h>
#include <trx/game/console/common.h>
#include <trx/game/console/registry.h>
#include <trx/game/game.h>
#include <trx/game/game_strings/entries.h>
#include <trx/game/lara/common.h>
#include <trx/game/lara/const.h>
#include <trx/game/objects/common.h>

static COMMAND_RESULT M_Entrypoint(const COMMAND_CONTEXT *const ctx)
{
    if (!Game_IsPlayable()) {
        return CR_UNAVAILABLE;
    }

    ITEM *const lara_item = Lara_GetItem();
    if (String_IsEmpty(ctx->args)) {
        Console_Log(
            GS("general/osd/current_health_get"), lara_item->hit_points);
        return CR_SUCCESS;
    }

    int32_t hp;
    if (!String_ParseInteger(ctx->args, &hp)) {
        return CR_BAD_INVOCATION;
    }
    CLAMP(hp, 0, LARA_MAX_HITPOINTS);

    lara_item->hit_points = hp;
    Console_Log(GS("general/osd/current_health_set"), hp);
    return CR_SUCCESS;
}

REGISTER_CONSOLE_COMMAND("hp", M_Entrypoint, GS_ID("console/cmd/hp/help"))
