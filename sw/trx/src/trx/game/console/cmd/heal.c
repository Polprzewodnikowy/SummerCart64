#include <trx/core/strings.h>
#include <trx/game/console/common.h>
#include <trx/game/console/registry.h>
#include <trx/game/game.h>
#include <trx/game/game_strings/entries.h>
#include <trx/game/lara/common.h>
#include <trx/game/lara/const.h>
#include <trx/game/lara/misc.h>

static COMMAND_RESULT M_Entrypoint(const COMMAND_CONTEXT *const ctx)
{
    if (!String_IsEmpty(ctx->args)) {
        return CR_BAD_INVOCATION;
    }

    if (!Game_IsPlayable()) {
        return CR_UNAVAILABLE;
    }

    ITEM *const lara_item = Lara_GetItem();
    LARA_INFO *const lara = Lara_GetLaraInfo();

    if (lara_item->hit_points == LARA_MAX_HITPOINTS) {
        Console_LogWarning(GS("general/osd/heal_already_full_hp"));
    } else {
        Console_Log(GS("general/osd/heal_success"));
    }

    lara_item->hit_points = LARA_MAX_HITPOINTS;
    lara->poison_timer = 0;
    Lara_Extinguish();
    return CR_SUCCESS;
}

REGISTER_CONSOLE_COMMAND("heal", M_Entrypoint, GS_ID("console/cmd/heal/help"))
