#include <trx/core/strings.h>
#include <trx/game/console/common.h>
#include <trx/game/console/registry.h>
#include <trx/game/game_flow/common.h>
#include <trx/game/game_strings/entries.h>

static const GF_LEVEL *M_FindLevel(const char *const user_input)
{
    int32_t level_num = -1;
    if (String_ParseInteger(user_input, &level_num)) {
        return GF_GetLevelByOrdinalNumber(GFLT_MAIN, level_num);
    }

    VECTOR *source = Vector_Create(sizeof(STRING_FUZZY_SOURCE));
    for (int32_t i = 0; i < GF_GetLevelTable(GFLT_MAIN)->count; i++) {
        const GF_LEVEL *const level = GF_GetLevel(GFLT_MAIN, i);
        STRING_FUZZY_SOURCE source_item = {
            .key = level->title,
            .value = (void *)level,
            .weight = 1,
        };
        if (source_item.key != nullptr) {
            Vector_Add(source, &source_item);
        }
    }

    const GF_LEVEL *const gym_level = GF_GetGymLevel();
    if (gym_level != nullptr) {
        STRING_FUZZY_SOURCE source_item = {
            .key = "gym",
            .value = (void *)gym_level,
            .weight = 1,
        };
        Vector_Add(source, &source_item);
    }

    const GF_LEVEL *result = nullptr;
    VECTOR *matches = String_FuzzyMatch(user_input, source);
    if (matches->count >= 1) {
        const STRING_FUZZY_MATCH *const match = Vector_Get(matches, 0);
        result = (const GF_LEVEL *)match->value;
    }

    if (matches != nullptr) {
        Vector_Free(matches);
        matches = nullptr;
    }
    if (source != nullptr) {
        Vector_Free(source);
        source = nullptr;
    }
    return result;
}

static COMMAND_RESULT M_Entrypoint(const COMMAND_CONTEXT *const ctx)
{
    if (String_IsEmpty(ctx->args)) {
        return CR_BAD_INVOCATION;
    }

    VECTOR *matches = nullptr;
    const GF_LEVEL *const level = M_FindLevel(ctx->args);

    if (level == nullptr || level->type == GFL_DUMMY
        || level->type == GFL_CURRENT) {
        Console_LogError(GS("general/osd/invalid_level"));
        return CR_FAILURE;
    }

    GF_OverrideCommand((GF_COMMAND) {
        .action = GF_SELECT_GAME,
        .param = level->num,
    });
    Console_Log(GS("general/osd/play_level"), level->title);
    return CR_SUCCESS;
}

REGISTER_CONSOLE_COMMAND(
    "play", M_Entrypoint, GS_ID("console/cmd/play_level/help"))
REGISTER_CONSOLE_COMMAND(
    "level", M_Entrypoint, GS_ID("console/cmd/play_level/help"))
