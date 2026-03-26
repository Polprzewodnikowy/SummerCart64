#include <trx/core/strings.h>
#include <trx/game/console/registry.h>
#include <trx/game/lara/common.h>
#include <trx/game/rooms.h>

static COMMAND_RESULT M_Entrypoint(const COMMAND_CONTEXT *const ctx)
{
    int32_t room_num;
    if (!String_ParseInteger(ctx->args, &room_num)) {
        if (!String_IsEmpty(ctx->args)) {
            return CR_BAD_INVOCATION;
        }

        ITEM *const lara_item = Lara_GetItem();
        if (lara_item == nullptr) {
            return CR_UNAVAILABLE;
        }
        room_num = lara_item->room_num;
    }

    if (String_Equivalent(ctx->prefix, "flood")) {
        Room_Get(room_num)->flags.underwater = true;
    } else if (String_Equivalent(ctx->prefix, "drain")) {
        Room_Get(room_num)->flags.underwater = false;
    } else {
        return CR_UNAVAILABLE;
    }

    return CR_SUCCESS;
}

REGISTER_CONSOLE_COMMAND("flood", M_Entrypoint, GS_ID("console/cmd/flood/help"))
REGISTER_CONSOLE_COMMAND("drain", M_Entrypoint, GS_ID("console/cmd/drain/help"))
