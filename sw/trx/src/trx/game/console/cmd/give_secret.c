#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/debug.h>
#include <trx/game/console/common.h>
#include <trx/game/console/registry.h>
#include <trx/game/game.h>
#include <trx/game/game_strings/entries.h>
#include <trx/game/savegame.h>
#include <trx/game/stats.h>
#include <trx/game/stats/common.h>

#include <stdio.h>
#include <string.h>

#define M_FMT_NUM "#%d"

static const char *M_FormatAvailable(void)
{
    LEVEL_MAX_STATS *const max_stats =
        Stats_GetLevelMaxStats(Game_GetCurrentLevel());
    ASSERT(max_stats != nullptr);

    char buf[128] = {};
    char *ptr = buf;
    bool first = true;
    for (int32_t i = 0; i < STATS_MAX_SECRETS; i++) {
        if ((1 << i) & max_stats->all_secrets_mask) {
            if (!first) {
                ptr += sprintf(ptr, ", ");
            }
            first = false;
            ptr += sprintf(ptr, M_FMT_NUM, i + 1);
        }
    }
    return String_FormatStatic("%s", buf);
}

static const char *M_FormatPresent(void)
{
    char buf[128] = {};
    char *ptr = buf;
    bool first = true;
    for (int32_t i = 0; i < STATS_MAX_SECRETS; i++) {
        if (Stats_HasSecret(i)) {
            if (!first) {
                ptr += sprintf(ptr, ", ");
            }
            first = false;
            ptr += sprintf(ptr, M_FMT_NUM, i + 1);
        }
    }
    return String_FormatStatic("%s", buf);
}

static void M_LogInvalid(const int32_t idx)
{
    Console_LogError(
        GS("console/cmd/give/invalid_secret"),
        String_FormatStatic(M_FMT_NUM, idx + 1), M_FormatAvailable());
}

static COMMAND_RESULT M_TakeSecret(const int32_t idx)
{
    if (Stats_RemoveSecret(idx)) {
        Console_Log(
            GS("console/cmd/give/secret_taken"),
            String_FormatStatic(M_FMT_NUM, idx + 1));
        return CR_SUCCESS;
    }
    M_LogInvalid(idx);
    return CR_FAILURE;
}

static COMMAND_RESULT M_GiveSecret(const int32_t idx)
{
    if (Stats_AddSecret(idx)) {
        Console_Log(
            GS("console/cmd/give/secret_given"),
            String_FormatStatic(M_FMT_NUM, idx + 1));
        return CR_SUCCESS;
    }
    M_LogInvalid(idx);
    return CR_FAILURE;
}

static COMMAND_RESULT M_ListSecrets(void)
{
    const LEVEL_MAX_STATS *const max_stats =
        Stats_GetLevelMaxStats(Game_GetCurrentLevel());
    ASSERT(max_stats != nullptr);
    RESUME_INFO *const info = Savegame_GetCurrentInfo(Game_GetCurrentLevel());
    ASSERT(info != nullptr);

    const char *const buf = M_FormatPresent();
    Console_Log(
        strcmp(buf, "") == 0 ? GS("console/cmd/give/secret_none")
                             : GS("console/cmd/give/secret_list"),
        info->stats.secret_count, max_stats->max_secret_count, buf);
    return CR_SUCCESS;
}

static COMMAND_RESULT M_GiveAllSecrets(void)
{
    for (int32_t i = 0; i < STATS_MAX_SECRETS; i++) {
        Stats_AddSecret(i); // Not all `i` are valid, but it's handled inside
    }
    return M_ListSecrets();
}

static COMMAND_RESULT M_TakeAllSecrets(void)
{
    for (int32_t i = 0; i < STATS_MAX_SECRETS; i++) {
        Stats_RemoveSecret(i); // Not all `i` are valid, but it's handled inside
    }
    return M_ListSecrets();
}

static COMMAND_RESULT M_Entrypoint(const COMMAND_CONTEXT *const ctx)
{
    if (!Game_IsPlayable()) {
        return CR_UNAVAILABLE;
    }

    if (String_IsEmpty(ctx->args)) {
        return M_ListSecrets();
    }

    if (String_Equivalent(ctx->args, "give")) {
        return M_GiveAllSecrets();
    }

    if (String_Equivalent(ctx->args, "take")) {
        return M_TakeAllSecrets();
    }

    char *args = Memory_DupStr(ctx->args);
    char *subcmd = args;
    char *param = nullptr;

    if (!String_IsEmpty(ctx->args)) {
        param = strchr(args, ' ');
        if (param != nullptr) {
            *param = '\0';
            param++;
        }
    }

    if (param == nullptr || String_IsEmpty(param)) {
        Memory_FreePointer(&args);
        return CR_BAD_INVOCATION;
    }

    int32_t num;
    if (!String_ParseInteger(param, &num)) {
        Memory_FreePointer(&args);
        return CR_BAD_INVOCATION;
    }

    COMMAND_RESULT result = CR_FAILURE;
    if (String_Equivalent(subcmd, "take")) {
        result = M_TakeSecret(num - 1);
    } else if (String_Equivalent(subcmd, "give")) {
        result = M_GiveSecret(num - 1);
    } else {
        result = CR_BAD_INVOCATION;
    }

    Memory_FreePointer(&args);
    return result;
}

REGISTER_CONSOLE_COMMAND(
    "secret", M_Entrypoint, GS_ID("console/cmd/give_secret/help"))
