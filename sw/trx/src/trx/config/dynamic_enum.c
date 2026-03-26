#include <trx/config/dynamic_enum.h>

#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/core/vector.h>
#include <trx/debug.h>
#include <trx/game/game_strings/entries.h>

#include <string.h>

typedef struct {
    char *value;
    char *label;
} M_DYNAMIC_ENUM_VALUE;

typedef struct M_DYNAMIC_ENUM_REGISTRY_ENTRY {
    const void *target;
    VECTOR *values;
    struct M_DYNAMIC_ENUM_REGISTRY_ENTRY *next;
} M_DYNAMIC_ENUM_REGISTRY_ENTRY;

static M_DYNAMIC_ENUM_REGISTRY_ENTRY *m_Registry = nullptr;

static bool M_IsDynamicEnum(const CONFIG_OPTION *const option)
{
    return option != nullptr && option->type == COT_DYNAMIC_ENUM;
}

static bool M_IsSameValue(const char *const left, const char *const right)
{
    if (left == nullptr && right == nullptr) {
        return true;
    }
    if (left == nullptr || right == nullptr) {
        return false;
    }
    return strcmp(left, right) == 0;
}

static M_DYNAMIC_ENUM_REGISTRY_ENTRY *M_GetRegistryEntry(
    const CONFIG_OPTION *const option, const bool create)
{
    if (option == nullptr) {
        return nullptr;
    }

    for (M_DYNAMIC_ENUM_REGISTRY_ENTRY *entry = m_Registry; entry != nullptr;
         entry = entry->next) {
        if (entry->target == option->target) {
            return entry;
        }
    }

    if (!create) {
        return nullptr;
    }

    M_DYNAMIC_ENUM_REGISTRY_ENTRY *const entry = Memory_Alloc(sizeof(*entry));
    entry->target = option->target;
    entry->values = Vector_Create(sizeof(M_DYNAMIC_ENUM_VALUE));
    entry->next = m_Registry;
    m_Registry = entry;
    return entry;
}

static void M_FreeValues(VECTOR **const values_ptr)
{
    if (values_ptr == nullptr || *values_ptr == nullptr) {
        return;
    }

    VECTOR *const values = *values_ptr;
    for (int32_t i = 0; i < values->count; i++) {
        M_DYNAMIC_ENUM_VALUE *const dyn_value = Vector_Get(values, i);
        Memory_FreePointer(&dyn_value->value);
        Memory_FreePointer(&dyn_value->label);
    }
    Vector_Free(values);
    *values_ptr = nullptr;
}

static int32_t M_FindValueIndex(
    const CONFIG_OPTION *const option, const char *const value)
{
    const M_DYNAMIC_ENUM_REGISTRY_ENTRY *const entry =
        M_GetRegistryEntry(option, false);
    if (entry == nullptr || entry->values == nullptr) {
        return -1;
    }

    for (int32_t i = 0; i < entry->values->count; i++) {
        const M_DYNAMIC_ENUM_VALUE *const dyn_value =
            Vector_Get(entry->values, i);
        if (M_IsSameValue(dyn_value->value, value)) {
            return i;
        }
    }

    return -1;
}

static const M_DYNAMIC_ENUM_VALUE *M_GetValueEntry(
    const CONFIG_OPTION *const option, const int32_t index)
{
    const M_DYNAMIC_ENUM_REGISTRY_ENTRY *const entry =
        M_GetRegistryEntry(option, false);
    if (entry == nullptr || entry->values == nullptr) {
        return nullptr;
    }
    if (index < 0 || index >= entry->values->count) {
        return nullptr;
    }
    return Vector_Get(entry->values, index);
}

static const char *M_GetDisplayLabel(
    const M_DYNAMIC_ENUM_VALUE *const dyn_value)
{
    if (dyn_value == nullptr) {
        return "(null)";
    }
    if (!String_IsEmpty(dyn_value->label)) {
        const char *const resolved = GameString_Get(dyn_value->label);
        if (!String_IsEmpty(resolved)) {
            return resolved;
        }
    }
    if (dyn_value->value != nullptr) {
        return dyn_value->value;
    }
    return "(null)";
}

static void M_Shutdown(void)
{
    M_DYNAMIC_ENUM_REGISTRY_ENTRY *entry = m_Registry;
    while (entry != nullptr) {
        M_DYNAMIC_ENUM_REGISTRY_ENTRY *const next = entry->next;
        M_FreeValues(&entry->values);
        Memory_FreePointer(&entry);
        entry = next;
    }
    m_Registry = nullptr;
}

__attribute__((destructor)) static void M_AtShutdown(void)
{
    M_Shutdown();
}

void Config_DynamicEnum_ResetValues(const CONFIG_OPTION *const option)
{
    if (!M_IsDynamicEnum(option)) {
        return;
    }

    M_DYNAMIC_ENUM_REGISTRY_ENTRY *const entry =
        M_GetRegistryEntry(option, true);
    ASSERT(entry != nullptr);
    M_FreeValues(&entry->values);
    entry->values = Vector_Create(sizeof(M_DYNAMIC_ENUM_VALUE));
}

bool Config_DynamicEnum_AddValue(
    const CONFIG_OPTION *const option, const char *const value,
    const char *const label)
{
    if (!M_IsDynamicEnum(option)) {
        return false;
    }

    M_DYNAMIC_ENUM_REGISTRY_ENTRY *const entry =
        M_GetRegistryEntry(option, true);
    ASSERT(entry != nullptr);
    if (entry->values == nullptr) {
        entry->values = Vector_Create(sizeof(M_DYNAMIC_ENUM_VALUE));
    }

    M_DYNAMIC_ENUM_VALUE dyn_value = {
        .value = value != nullptr ? Memory_DupStr(value) : nullptr,
        .label = label != nullptr ? Memory_DupStr(label) : nullptr,
    };
    Vector_Add(entry->values, &dyn_value);
    return true;
}

bool Config_DynamicEnum_IsValidValue(
    const CONFIG_OPTION *const option, const char *const value)
{
    return M_FindValueIndex(option, value) >= 0;
}

int32_t Config_DynamicEnum_GetValueCount(const CONFIG_OPTION *const option)
{
    const M_DYNAMIC_ENUM_REGISTRY_ENTRY *const entry =
        M_GetRegistryEntry(option, false);
    if (entry == nullptr || entry->values == nullptr) {
        return 0;
    }
    return entry->values->count;
}

const char *Config_DynamicEnum_GetValueAt(
    const CONFIG_OPTION *const option, const int32_t index)
{
    const M_DYNAMIC_ENUM_VALUE *const dyn_value =
        M_GetValueEntry(option, index);
    if (dyn_value == nullptr) {
        return nullptr;
    }
    return dyn_value->value;
}

const char *Config_DynamicEnum_GetLabelAt(
    const CONFIG_OPTION *const option, const int32_t index)
{
    const M_DYNAMIC_ENUM_VALUE *const dyn_value =
        M_GetValueEntry(option, index);
    return M_GetDisplayLabel(dyn_value);
}

const char *Config_DynamicEnum_GetLabelForValue(
    const CONFIG_OPTION *const option, const char *const value)
{
    const int32_t idx = M_FindValueIndex(option, value);
    if (idx < 0) {
        return value != nullptr ? value : "(null)";
    }
    return Config_DynamicEnum_GetLabelAt(option, idx);
}

bool Config_DynamicEnum_CanCycle(
    const CONFIG_OPTION *const option, const char *const current,
    const int32_t dir)
{
    if (!M_IsDynamicEnum(option) || dir == 0) {
        return false;
    }

    const int32_t value_count = Config_DynamicEnum_GetValueCount(option);
    if (value_count <= 0) {
        return false;
    }

    const int32_t cur_idx = M_FindValueIndex(option, current);
    if (cur_idx < 0) {
        return true;
    }

    const int32_t step = dir < 0 ? -1 : 1;
    const int32_t next_idx = cur_idx + step;
    return next_idx >= 0 && next_idx < value_count;
}

const char *Config_DynamicEnum_GetNext(
    const CONFIG_OPTION *const option, const char *const current,
    const int32_t dir)
{
    if (!M_IsDynamicEnum(option) || dir == 0) {
        return nullptr;
    }

    const int32_t value_count = Config_DynamicEnum_GetValueCount(option);
    if (value_count <= 0) {
        return nullptr;
    }

    const int32_t cur_idx = M_FindValueIndex(option, current);
    if (cur_idx < 0) {
        return Config_DynamicEnum_GetValueAt(option, 0);
    }

    const int32_t step = dir < 0 ? -1 : 1;
    const int32_t next_idx = cur_idx + step;
    if (next_idx < 0 || next_idx >= value_count) {
        return nullptr;
    }

    return Config_DynamicEnum_GetValueAt(option, next_idx);
}
