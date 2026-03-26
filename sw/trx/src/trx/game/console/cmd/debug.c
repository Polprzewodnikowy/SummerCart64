#include <trx/config.h>
#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/debug.h>
#include <trx/game/console/cmd/config.h>
#include <trx/game/console/registry.h>
#include <trx/game/game_strings/entries.h>

#include <string.h>

typedef struct {
    bool *target;
    const CONFIG_OPTION *option;
} DEBUG_OPTION_ENTRY;

static DEBUG_OPTION_ENTRY m_AllOptions[] = {
    { &g_Config.debug.enable_debug_portals, nullptr },
    { &g_Config.debug.enable_debug_room_clip, nullptr },
    { &g_Config.debug.enable_debug_triggers, nullptr },
    { &g_Config.debug.enable_debug_spheres, nullptr },
    { &g_Config.debug.enable_debug_bounding_boxes, nullptr },
    { &g_Config.debug.enable_debug_pos, nullptr },
    { &g_Config.debug.enable_debug_anim, nullptr },
    { &g_Config.debug.enable_debug_camera, nullptr },
    { &g_Config.debug.enable_debug_status, nullptr },
    { nullptr, nullptr }
};

static void M_InitOptions(void)
{
    for (int32_t i = 0; m_AllOptions[i].target; i++) {
        m_AllOptions[i].option =
            Console_Cmd_Config_GetOptionFromTarget(m_AllOptions[i].target);
    }
}

static VECTOR *M_BuildMatches(const char *const key)
{
    VECTOR *const matches = Console_Cmd_Config_GetOptionsFromKey(key);
    VECTOR *const filtered = Vector_Create(sizeof(STRING_FUZZY_MATCH));

    for (int32_t i = 0; i < matches->count; i++) {
        const STRING_FUZZY_MATCH *const match = Vector_Get(matches, i);
        const CONFIG_OPTION *const option = match->value;
        for (int32_t j = 0; m_AllOptions[j].target; j++) {
            if (option == m_AllOptions[j].option) {
                Vector_Add(filtered, match);
                break;
            }
        }
    }

    Vector_Free(matches);
    return filtered;
}

static void M_LogOption(const CONFIG_OPTION *const option)
{
    char *const name = Console_Cmd_Config_NormalizeKey(option->name);
    Console_Log(
        GS("general/osd/config_option_set"), name,
        Config_GetOptionValueAsString(option, false));
    Memory_Free(name);
}

static void M_ShowOption(const CONFIG_OPTION *const option)
{
    char *const name = Console_Cmd_Config_NormalizeKey(option->name);
    Console_Log(
        GS("general/osd/config_option_get"), name,
        Config_GetOptionValueAsString(option, false));
    Memory_Free(name);
}

static bool M_UpdateOption(const CONFIG_OPTION *const option, const bool enable)
{
    bool *const target = (bool *)option->target;
    if (*target == enable) {
        return false;
    }

    *target = enable;
    M_LogOption(option);
    return true;
}

static bool M_UpdateAll(const bool enable)
{
    bool changed = false;
    for (int32_t i = 0; m_AllOptions[i].target != nullptr; i++) {
        if (M_UpdateOption(m_AllOptions[i].option, enable)) {
            changed = true;
        }
    }
    return changed;
}

static void M_ShowStatus(void)
{
    for (int32_t i = 0; m_AllOptions[i].target != nullptr; i++) {
        M_ShowOption(m_AllOptions[i].option);
    }
}

static COMMAND_RESULT M_Entrypoint(const COMMAND_CONTEXT *ctx)
{
    if (m_AllOptions[0].option == nullptr) {
        M_InitOptions();
    }

    if (String_IsEmpty(ctx->args)) {
        M_ShowStatus();
        return CR_SUCCESS;
    }

    char *args = Memory_DupStr(ctx->args);
    char *space = strchr(args, ' ');
    char *key = args;
    char *val = nullptr;

    if (space) {
        *space = '\0';
        val = space + 1;
    }
    if (val != nullptr && strchr(val, ' ') != nullptr) {
        Memory_Free(args);
        return CR_BAD_INVOCATION;
    }

    bool use_set = false;
    bool explicit_enable = false;
    if (val == nullptr && String_ParseBool(key, &explicit_enable)) {
        if (M_UpdateAll(explicit_enable)) {
            Config_Update();
        }
        Memory_Free(args);
        return CR_SUCCESS;
    } else if (val != nullptr && String_ParseBool(val, &explicit_enable)) {
        use_set = true;
    } else if (val != nullptr) {
        Memory_Free(args);
        return CR_BAD_INVOCATION;
    }

    VECTOR *const matches = M_BuildMatches(key);
    if (matches->count == 0) {
        Console_LogError(GS("general/osd/config_option_unknown_option"), key);
        Vector_Free(matches);
        Memory_Free(args);
        return CR_FAILURE;
    }

    bool changed = false;
    for (int32_t i = 0; i < matches->count; i++) {
        const STRING_FUZZY_MATCH *const match = Vector_Get(matches, i);
        const CONFIG_OPTION *const option = match->value;
        const bool enable =
            use_set ? explicit_enable : !*(bool *)option->target;
        if (M_UpdateOption(option, enable)) {
            changed = true;
        }
    }

    if (changed) {
        Config_Update();
    }
    Vector_Free(matches);
    Memory_Free(args);
    return CR_SUCCESS;
}

REGISTER_CONSOLE_COMMAND("debug", M_Entrypoint, GS_ID("console/cmd/debug/help"))
