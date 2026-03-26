#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/game/console/common.h>
#include <trx/game/console/registry.h>
#include <trx/game/const.h>
#include <trx/game/game.h>
#include <trx/game/game_flow/common.h>
#include <trx/game/game_strings/entries.h>
#include <trx/game/lara/common.h>
#include <trx/game/objects/common.h>
#include <trx/game/rooms.h>

#include <string.h>

static COMMAND_RESULT M_Entrypoint(const COMMAND_CONTEXT *const ctx)
{
    if (!Game_IsLoaded()) {
        return CR_UNAVAILABLE;
    }

    if (!String_IsEmpty(ctx->args)) {
        return CR_BAD_INVOCATION;
    }

    const GF_LEVEL *const current_level = GF_GetCurrentLevel();
    if (current_level->type == GFL_TITLE) {
        return CR_UNAVAILABLE;
    }

    const char *level_type_fmt = nullptr;
    int32_t reindex = 0;
    switch (current_level->type) {
    case GFL_CUTSCENE:
        level_type_fmt = GS("general/osd/pos_level_fmt_cutscene");
        reindex = 1;
        break;
    case GFL_DEMO:
        level_type_fmt = GS("general/osd/pos_level_fmt_demo");
        reindex = 1;
        break;
    default:
        level_type_fmt = GS("general/osd/pos_level_fmt");
        reindex = GF_GetGymLevel() == nullptr ? 1 : 0;
        break;
    }

    const char *const level_type =
        String_FormatStatic(level_type_fmt, current_level->num + reindex);

    const ITEM *const lara_item = Lara_GetItem();
    const char *details;
    if (lara_item == nullptr) {
        details = String_FormatStatic("%s", GS("general/osd/pos_lara_missing"));
    } else {
        int16_t room_num = lara_item->room_num;
        const ROOM *const room = Room_Get(room_num);
        if (Room_GetFlipStatus() && room->flipped_room != NO_ROOM) {
            room_num = room->flipped_room;
        }
        details = String_FormatStatic(
            GS("general/osd/pos_lara_pos_fmt"), room_num,
            lara_item->pos.x / (float)WALL_L, lara_item->pos.y / (float)WALL_L,
            lara_item->pos.z / (float)WALL_L,
            lara_item->rot.x * 360.0f / (float)DEG_360,
            lara_item->rot.y * 360.0f / (float)DEG_360,
            lara_item->rot.z * 360.0f / (float)DEG_360);
    }
    const char *const glue = lara_item == nullptr ? "\n" : "  ";

    const char *const message = current_level->title != nullptr
            && strcmp(level_type, current_level->title) == 0
        ? String_FormatStatic("%s%s%s", level_type, glue, details)
        : String_FormatStatic(
              "%s (%s)%s%s", level_type, current_level->title, glue, details);

    Console_Log("%s", message);

    return CR_SUCCESS;
}

REGISTER_CONSOLE_COMMAND("pos", M_Entrypoint, GS_ID("console/cmd/pos/help"))
