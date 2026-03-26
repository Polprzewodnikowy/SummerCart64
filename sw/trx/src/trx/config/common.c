#include <trx/config/common.h>

#include <trx/config/dynamic_enum.h>
#include <trx/config/file.h>
#include <trx/config/priv.h>
#include <trx/config/vars.h>
#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/debug.h>
#include <trx/game/game_flow/vars.h>
#include <trx/game/game_strings/entries.h>
#include <trx/game/shell.h>

#include <stdio.h>
#include <string.h>

// In-memory list of pointers to config options enforced by the game flow.
static VECTOR *m_EnforcedOptions = nullptr;
// In-memory list of pointers to config options hidden by the game flow.
static VECTOR *m_HiddenOptions = nullptr;

static EVENT_MANAGER *m_EventManager = nullptr;

static void M_FreeStringOptionValues(void)
{
    const CONFIG_OPTION *option = Config_GetOptionMap();
    while (option != nullptr && option->target != nullptr) {
        if (option->type == COT_STRING || option->type == COT_DYNAMIC_ENUM) {
            Memory_Free(*(char **)option->target);
        }
        option++;
    }
}

__attribute__((constructor)) static void M_Init(void)
{
    m_EventManager = EventManager_Create();
}

__attribute__((destructor)) static void M_Shutdown(void)
{
    EventManager_Free(m_EventManager);
    m_EventManager = nullptr;

    M_FreeStringOptionValues();
    Memory_FreePointer(&g_Config.default_path);
    Memory_FreePointer(&g_Config.enforced_path);

    if (m_EnforcedOptions != nullptr) {
        Vector_Free(m_EnforcedOptions);
        m_EnforcedOptions = nullptr;
    }
    if (m_HiddenOptions != nullptr) {
        Vector_Free(m_HiddenOptions);
        m_HiddenOptions = nullptr;
    }
}

void Config_ApplyDefaultSettings(void)
{
    const CONFIG_OPTION *option = Config_GetOptionMap();
    while (option->target != nullptr) {
        Config_RestoreOptionDefault(option->target);
        option++;
    }
}

bool Config_Read(
    const char *const default_path, const char *const enforced_path)
{
    // Always initialize the config, even if the file is missing, so that
    // the game can interact with these properties.
    Memory_FreePointer(&g_Config.default_path);
    Memory_FreePointer(&g_Config.enforced_path);
    g_Config.default_path = Memory_DupStr(default_path);
    g_Config.enforced_path = Memory_DupStr(enforced_path);
    g_Config.loaded = true;

    LOG_DEBUG("Reading config");
    LOG_DEBUG("  default_path=%s", g_Config.default_path);
    LOG_DEBUG("  enforced_path=%s", g_Config.enforced_path);
    if (m_EnforcedOptions == nullptr) {
        m_EnforcedOptions = Vector_Create(sizeof(void *));
    } else {
        Vector_ClearRealloc(m_EnforcedOptions);
    }
    if (m_HiddenOptions == nullptr) {
        m_HiddenOptions = Vector_Create(sizeof(void *));
    } else {
        Vector_ClearRealloc(m_HiddenOptions);
    }
    const CONFIG_IO_ARGS args = {
        .default_path = g_Config.default_path,
        .enforced_path = g_Config.enforced_path,
        .action = &Config_LoadFromJSON,
        .enforced_targets = m_EnforcedOptions,
        .hidden_targets = m_HiddenOptions,
    };
    const bool result = ConfigFile_Read(&args);
    if (result) {
        LOG_DEBUG("Config loaded");
    } else {
        LOG_WARNING("Errors while loading config");
    }
    Config_Sanitize();
    g_SavedConfig = g_Config;
    return result;
}

bool Config_Update(void)
{
    Config_Sanitize();
    if (memcmp(&g_Config, &g_SavedConfig, sizeof(CONFIG)) == 0) {
        return false;
    }

    if (m_EventManager != nullptr) {
        const EVENT event = {
            .name = "change",
            .sender = nullptr,
            .data = nullptr,
        };
        EventManager_Fire(m_EventManager, &event);
    }
    g_Config.dirty = false;
    g_SavedConfig = g_Config;
    return true;
}

bool Config_Write(void)
{
    ASSERT(g_Config.default_path != nullptr);
    const CONFIG_IO_ARGS args = {
        .default_path = g_Config.default_path,
        .enforced_path = g_Config.enforced_path,
        .action = &Config_DumpToJSON,
    };
    return ConfigFile_Write(&args);
}

int32_t Config_SubscribeChanges(
    const EVENT_LISTENER listener, void *const user_data)
{
    ASSERT(m_EventManager != nullptr);
    return EventManager_Subscribe(
        m_EventManager, "change", nullptr, listener, user_data);
}

void Config_UnsubscribeChanges(const int32_t listener_id)
{
    ASSERT(m_EventManager != nullptr);
    EventManager_Unsubscribe(m_EventManager, listener_id);
}

const CONFIG_OPTION *Config_GetOption(const void *const target)
{
    const CONFIG_OPTION *option = Config_GetOptionMap();
    if (option == nullptr) {
        return nullptr;
    }
    while (option->target != nullptr) {
        if (option->target == target) {
            return option;
        }
        option++;
    }
    return nullptr;
}

bool Config_IsOptionEnforced(const void *const target)
{
    return m_EnforcedOptions != nullptr
        && Vector_Contains(m_EnforcedOptions, &target);
}

bool Config_IsOptionHidden(const void *const target)
{
    return m_HiddenOptions != nullptr
        && Vector_Contains(m_HiddenOptions, &target);
}

bool Config_IsOptionAtDefault(const void *const target)
{
    const CONFIG_OPTION *option = Config_GetOption(target);
    if (target == nullptr) {
        return true;
    }
    switch (option->type) {
    case COT_BOOL:
        return *(bool *)option->target == *(bool *)option->default_value;
    case COT_INT32:
        return *(int32_t *)option->target == *(int32_t *)option->default_value;
    case COT_FLOAT:
    case COT_FLOAT_PERCENT:
        return *(float *)option->target == *(float *)option->default_value;
    case COT_DOUBLE:
        return *(double *)option->target == *(double *)option->default_value;
    case COT_RGB888: {
        const RGB_888 cur = *(RGB_888 *)option->target;
        const RGB_888 def = *(RGB_888 *)option->default_value;
        return cur.r == def.r && cur.g == def.g && cur.b == def.b;
    }
    case COT_ENUM:
        return *(int32_t *)option->target == *(int32_t *)option->default_value;
        break;
    case COT_STRING:
    case COT_DYNAMIC_ENUM: {
        const char *const cur = *(char **)option->target;
        const char *const def = (const char *)option->default_value;
        if (cur == nullptr && def == nullptr) {
            return true;
        }
        if (cur == nullptr || def == nullptr) {
            return false;
        }
        return strcmp(cur, def) == 0;
    }
    }
    return true;
}

bool Config_RestoreOptionDefault(const void *const target)
{
    const CONFIG_OPTION *option = Config_GetOption(target);
    if (target == nullptr) {
        return false;
    }
    switch (option->type) {
    case COT_BOOL:
        *(bool *)option->target = *(bool *)option->default_value;
        return true;
    case COT_INT32:
        *(int32_t *)option->target = *(int32_t *)option->default_value;
        return true;
    case COT_FLOAT:
    case COT_FLOAT_PERCENT:
        *(float *)option->target = *(float *)option->default_value;
        return true;
    case COT_DOUBLE:
        *(double *)option->target = *(double *)option->default_value;
        return true;
    case COT_RGB888:
        *(RGB_888 *)option->target = *(RGB_888 *)option->default_value;
        return true;
    case COT_ENUM:
        *(int32_t *)option->target = *(int32_t *)option->default_value;
        return true;
    case COT_STRING:
    case COT_DYNAMIC_ENUM: {
        char **const p = (char **)option->target;
        const char *const def = (const char *)option->default_value;
        char *const old = *p;
        *p = def != nullptr ? Memory_DupStr(def) : nullptr;
        // VERY IMPORTANT: free the memory AFTER we allocate, so that we force
        // the pointer to get a different macro, so that change subscribers
        // can see the string has changed by comparing just the pointers.
        Memory_Free(old);
        return true;
    }
    }
    return false;
}

static bool M_ParseBool(const char *const value, bool *const result)
{
    if (String_Match(value, "^(on|true|1)$")) {
        *result = true;
        return true;
    }
    if (String_Match(value, "^(off|false|0)$")) {
        *result = false;
        return true;
    }
    return false;
}

static bool M_ParseInt32(const char *const value, int32_t *const result)
{
    return sscanf(value, "%d", result) == 1;
}

static bool M_ParseFloat(const char *const value, float *const result)
{
    return sscanf(value, "%f", result) == 1;
}

static bool M_ParseDouble(const char *const value, double *const result)
{
    return sscanf(value, "%lf", result) == 1;
}

static bool M_ParseEnum(
    const CONFIG_OPTION *const option, const char *const value,
    const bool allow_numeric, int32_t *const result)
{
    const int32_t mapped = EnumMap_Get(option->param, value, -1);
    if (mapped != -1) {
        *result = mapped;
        return true;
    }
    if (allow_numeric) {
        return M_ParseInt32(value, result);
    }
    return false;
}

static bool M_ParseRGB888(const char *const value, RGB_888 *const result)
{
    return String_ParseRGB888(value, result);
}

static const char *M_FormatBool(const bool value)
{
    return String_FormatStatic("%d", value);
}

static const char *M_FormatBoolHuman(const bool value)
{
    return value ? GS("general/misc/on") : GS("general/misc/off");
}

static const char *M_FormatInt32(const int32_t value)
{
    return String_FormatStatic("%d", value);
}

static const char *M_FormatFloat(const float value)
{
    return String_FormatStatic("%.2f", value);
}

static const char *M_FormatFloatPercent(const float value)
{
    return String_FormatStatic("%.0f%%", value);
}

static const char *M_FormatDouble(const double value)
{
    return String_FormatStatic("%.2f", value);
}

static const char *M_FormatEnumMachine(
    const CONFIG_OPTION *const option, const int32_t value)
{
    return String_FormatStatic("%s", EnumMap_ToString(option->param, value));
}

static const char *M_FormatEnumHuman(
    const CONFIG_OPTION *const option, const int32_t value)
{
    const char *const localized = EnumMap_GetLabel(option->param, value);
    ASSERT(localized != nullptr);
    return localized;
}

static const char *M_FormatRGB888(const RGB_888 *const value)
{
    return String_FormatStatic(
        "%02hhx%02hhx%02hhx", value->r, value->g, value->b);
}

static const char *M_FormatString(const char *const value)
{
    return String_FormatStatic("%s", value != nullptr ? value : "");
}

const char *Config_GetOptionValueAsString(
    const CONFIG_OPTION *const option, const bool human_readable)
{
    if (option == nullptr) {
        return nullptr;
    }
    switch (option->type) {
    case COT_BOOL:
        return human_readable ? M_FormatBoolHuman(*(bool *)option->target)
                              : M_FormatBool(*(bool *)option->target);
    case COT_INT32:
        return M_FormatInt32(*(int32_t *)option->target);
    case COT_FLOAT:
        return M_FormatFloat(*(float *)option->target);
    case COT_FLOAT_PERCENT:
        return M_FormatFloatPercent((*(float *)option->target) * 100.0f);
    case COT_DOUBLE:
        return M_FormatDouble(*(double *)option->target);
    case COT_ENUM:
        return human_readable
            ? M_FormatEnumHuman(option, *(int32_t *)option->target)
            : M_FormatEnumMachine(option, *(int32_t *)option->target);
    case COT_RGB888:
        return M_FormatRGB888(option->target);
    case COT_STRING:
        return M_FormatString(*(char **)option->target);
    case COT_DYNAMIC_ENUM: {
        if (human_readable) {
            const char *const value = *(char **)option->target;
            const char *const label =
                Config_DynamicEnum_GetLabelForValue(option, value);
            if (label != nullptr) {
                return label;
            }
        }
        return M_FormatString(*(char **)option->target);
    }
    default:
        return nullptr;
    }
}

const char *Config_GetOptionTitle(const CONFIG_OPTION *const opt)
{
    if (opt == nullptr || opt->name == nullptr) {
        return nullptr;
    }
    return GameString_Get(String_FormatStatic("settings/%s/title", opt->name));
}

const char *Config_GetOptionDescription(const CONFIG_OPTION *const opt)
{
    if (opt == nullptr || opt->name == nullptr) {
        return nullptr;
    }
    return GameString_Get(
        String_FormatStatic("settings/%s/description", opt->name));
}

char *Config_NormalizeOptionValueString(
    const CONFIG_OPTION *const option, const char *const value,
    const bool human_readable)
{
    if (option == nullptr) {
        return Memory_DupStr(value != nullptr ? value : "");
    }

    const char *const input = value != nullptr ? value : "";

#define L_NORMALIZE_TYPED(type_, parse_expr_, format_expr_)                    \
    do {                                                                       \
        type_ parsed;                                                          \
        if (!(parse_expr_)) {                                                  \
            return Memory_DupStr(input);                                       \
        }                                                                      \
        return Memory_DupStr(format_expr_);                                    \
    } while (false)

    switch (option->type) {
    case COT_BOOL:
        L_NORMALIZE_TYPED(
            bool, M_ParseBool(input, &parsed),
            human_readable ? M_FormatBoolHuman(parsed) : M_FormatBool(parsed));
    case COT_INT32:
        L_NORMALIZE_TYPED(
            int32_t, M_ParseInt32(input, &parsed), M_FormatInt32(parsed));
    case COT_FLOAT:
        L_NORMALIZE_TYPED(
            float, M_ParseFloat(input, &parsed), M_FormatFloat(parsed));
    case COT_FLOAT_PERCENT:
        L_NORMALIZE_TYPED(
            float, M_ParseFloat(input, &parsed), M_FormatFloatPercent(parsed));
    case COT_DOUBLE:
        L_NORMALIZE_TYPED(
            double, M_ParseDouble(input, &parsed), M_FormatDouble(parsed));
    case COT_ENUM:
        L_NORMALIZE_TYPED(
            int32_t, M_ParseEnum(option, input, true, &parsed),
            human_readable ? M_FormatEnumHuman(option, parsed)
                           : M_FormatEnumMachine(option, parsed));
    case COT_RGB888:
        L_NORMALIZE_TYPED(
            RGB_888, M_ParseRGB888(input, &parsed), M_FormatRGB888(&parsed));
    case COT_STRING:
        return Memory_DupStr(M_FormatString(input));
    case COT_DYNAMIC_ENUM:
        if (!Config_DynamicEnum_IsValidValue(option, input)) {
            return Memory_DupStr(input);
        }
        if (human_readable) {
            const char *const label =
                Config_DynamicEnum_GetLabelForValue(option, input);
            if (label != nullptr) {
                return Memory_DupStr(label);
            }
        }
        return Memory_DupStr(M_FormatString(input));
    }
#undef L_NORMALIZE_TYPED

    return Memory_DupStr(input);
}

bool Config_SetOptionValueFromString(
    const CONFIG_OPTION *const option, const char *const new_value)
{
    ASSERT(option != nullptr);
    ASSERT(option->target != nullptr);
    switch (option->type) {
    case COT_BOOL: {
        bool parsed;
        if (M_ParseBool(new_value, &parsed)) {
            *(bool *)option->target = parsed;
            return true;
        }
        break;
    }

    case COT_INT32: {
        int32_t parsed;
        if (M_ParseInt32(new_value, &parsed)) {
            *(int32_t *)option->target = parsed;
            return true;
        }
        break;
    }

    case COT_FLOAT: {
        float parsed;
        if (M_ParseFloat(new_value, &parsed)) {
            *(float *)option->target = parsed;
            return true;
        }
        break;
    }

    case COT_FLOAT_PERCENT: {
        float parsed;
        if (M_ParseFloat(new_value, &parsed)) {
            *(float *)option->target = parsed / 100.0f;
            return true;
        }
        break;
    }

    case COT_DOUBLE: {
        double parsed;
        if (M_ParseDouble(new_value, &parsed)) {
            *(double *)option->target = parsed;
            return true;
        }
        break;
    }

    case COT_ENUM: {
        int32_t parsed;
        if (M_ParseEnum(option, new_value, false, &parsed)) {
            *(int32_t *)option->target = parsed;
            return true;
        }
        break;
    }

    case COT_RGB888: {
        RGB_888 parsed;
        if (M_ParseRGB888(new_value, &parsed)) {
            *(RGB_888 *)option->target = parsed;
            return true;
        }
        break;
    }

    case COT_STRING:
    case COT_DYNAMIC_ENUM: {
        if (option->type == COT_DYNAMIC_ENUM
            && !Config_DynamicEnum_IsValidValue(option, new_value)) {
            return false;
        }
        char **const p = (char **)option->target;
        char *const old = *p;
        *p = new_value != nullptr ? Memory_DupStr(new_value) : nullptr;
        // VERY IMPORTANT: free the memory AFTER we allocate, so that we force
        // the pointer to get a different macro, so that change subscribers
        // can see the string has changed by comparing just the pointers.
        Memory_Free(old);
        return true;
    }
    }

    return false;
}
