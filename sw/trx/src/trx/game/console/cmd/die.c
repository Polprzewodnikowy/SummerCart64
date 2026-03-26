#include <trx/core/strings.h>
#include <trx/game/console/registry.h>
#include <trx/game/game.h>
#include <trx/game/items.h>
#include <trx/game/lara/common.h>
#include <trx/game/objects/common.h>
#include <trx/game/objects/ids.h>
#include <trx/game/sound.h>

static COMMAND_RESULT M_Entrypoint(const COMMAND_CONTEXT *const ctx)
{
    if (!String_IsEmpty(ctx->args)) {
        return CR_BAD_INVOCATION;
    }

    if (!Game_IsPlayable()) {
        return CR_UNAVAILABLE;
    }

    LARA_INFO *const lara = Lara_GetLaraInfo();
    ITEM *const lara_item = Lara_GetItem();
    if (lara_item->hit_points <= 0) {
        return CR_UNAVAILABLE;
    }

    Sound_Effect(SFX_LARA_FALL, &lara_item->pos, SPM_NORMAL);
    Sound_Effect(SFX_EXPLOSION_1, &lara_item->pos, SPM_NORMAL);
    Item_Explode(lara->item_num, -1, 1);

    lara_item->hit_points = 0;
    lara_item->flags |= IF_ONE_SHOT;
    return CR_SUCCESS;
}

REGISTER_CONSOLE_COMMAND("abortion", M_Entrypoint, nullptr)
REGISTER_CONSOLE_COMMAND("natla-?s(uc|tin)ks", M_Entrypoint, nullptr)
