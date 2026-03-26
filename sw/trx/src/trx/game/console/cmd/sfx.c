#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/game/console/common.h>
#include <trx/game/console/registry.h>
#include <trx/game/game_strings/entries.h>
#include <trx/game/sound.h>

#include <stdio.h>
#include <string.h>

static char *M_CreateRangeString(void)
{
    size_t buffer_size = 64;
    char *result = Memory_Alloc(buffer_size);

    int32_t prev = -1;
    int32_t start = -1;
    const SAMPLE_ID max_id = Sound_GetMaxDirectSampleID();
    for (SAMPLE_ID i = 0; i <= max_id; i++) {
        const bool valid = Sound_IsAvailable_Direct(i);

        if (valid && start == -1) {
            start = i;
        }
        if (!valid && start != -1) {
            char temp[32];
            if (start == prev) {
                sprintf(temp, "%d, ", prev);
            } else {
                sprintf(temp, "%d-%d, ", start, prev);
            }

            const int32_t len = strlen(temp);
            if (strlen(result) + len >= buffer_size) {
                buffer_size *= 2;
                result = Memory_Realloc(result, buffer_size);
            }

            strcat(result, temp);
            start = -1;
        }

        if (valid) {
            prev = i;
        }
    }

    // Remove the trailing comma and space
    result[strlen(result) - 2] = '\0';

    return result;
}

static COMMAND_RESULT M_Entrypoint(const COMMAND_CONTEXT *const ctx)
{
    if (String_IsEmpty(ctx->args)) {
        char *ranges = M_CreateRangeString();
        Console_Log(GS("general/osd/sound_available_samples"), ranges);
        Memory_FreePointer(&ranges);
        return CR_SUCCESS;
    }

    SAMPLE_ID sfx_id;
    if (!String_ParseInteger(ctx->args, &sfx_id)) {
        return CR_BAD_INVOCATION;
    }

    if (!Sound_IsAvailable_Direct(sfx_id)) {
        Console_LogError(GS("general/osd/invalid_sample"), sfx_id);
        return CR_FAILURE;
    }

    Console_Log(GS("general/osd/sound_playing_sample"), sfx_id);
    Sound_Effect_Direct(sfx_id, nullptr, SPM_ALWAYS);
    return CR_SUCCESS;
}

REGISTER_CONSOLE_COMMAND("sfx", M_Entrypoint, GS_ID("console/cmd/sfx/help"))
