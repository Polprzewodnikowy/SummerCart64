#include <trx/core/enum_map.h>

#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/game/game_strings/entries.h>

#include <uthash.h>

typedef struct {
    char *key;
    int32_t value;
    UT_hash_handle hh;
} M_STR_TO_ID_ENTRY;

typedef struct {
    char *key;
    char *str_value;
    UT_hash_handle hh;
} M_ID_TO_STR_ENTRY;

static M_STR_TO_ID_ENTRY *m_Str2IdMap = nullptr;
static M_ID_TO_STR_ENTRY *m_Id2StrMap = nullptr;
static M_ID_TO_STR_ENTRY *m_Id2NameMap = nullptr;
static M_ID_TO_STR_ENTRY *m_Id2LabelKeyMap = nullptr;

static void M_ClearStr2IdMap(M_STR_TO_ID_ENTRY **map)
{
    M_STR_TO_ID_ENTRY *current, *tmp;
    HASH_ITER(hh, *map, current, tmp)
    {
        HASH_DEL(*map, current);
        Memory_Free(current->key);
        Memory_Free(current);
    }
}

static void M_ClearId2StrMap(M_ID_TO_STR_ENTRY **map)
{
    M_ID_TO_STR_ENTRY *current, *tmp;
    HASH_ITER(hh, *map, current, tmp)
    {
        HASH_DEL(*map, current);
        Memory_Free(current->str_value);
        Memory_Free(current->key);
        Memory_Free(current);
    }
}

static __attribute__((destructor)) void M_Shutdown(void)
{
    M_ClearStr2IdMap(&m_Str2IdMap);
    M_ClearId2StrMap(&m_Id2StrMap);
    M_ClearId2StrMap(&m_Id2NameMap);
    M_ClearId2StrMap(&m_Id2LabelKeyMap);
}

static void M_DefineStr2Id(
    M_STR_TO_ID_ENTRY **map, const char *const enum_type_name,
    const int32_t enum_value, const char *const str_value)
{
    const char *const key =
        String_FormatStatic("%s|%s", enum_type_name, str_value);
    M_STR_TO_ID_ENTRY *const entry = Memory_Alloc(sizeof(M_STR_TO_ID_ENTRY));
    entry->key = Memory_DupStr(key);
    entry->value = enum_value;
    HASH_ADD_KEYPTR(hh, *map, entry->key, strlen(entry->key), entry);
}

static void M_DefineId2Str(
    M_ID_TO_STR_ENTRY **map, const char *const enum_type_name,
    const int32_t enum_value, const char *const str_value)
{
    const char *const key =
        String_FormatStatic("%s|%d", enum_type_name, enum_value);
    M_ID_TO_STR_ENTRY *entry;
    HASH_FIND_STR(*map, key, entry);
    if (entry != nullptr) {
        // The inverse lookup is already defined - do not override it.
        // (This means that the first call to ENUM_MAP for a given enum value
        // also determines what serializing it back to string will pick
        // in the event there are multiple aliases).
        return;
    }

    entry = Memory_Alloc(sizeof(M_ID_TO_STR_ENTRY));
    entry->key = Memory_DupStr(key);
    entry->str_value = Memory_DupStr(str_value);
    HASH_ADD_KEYPTR(hh, *map, entry->key, strlen(entry->key), entry);
}

static int32_t M_Str2Id(
    M_STR_TO_ID_ENTRY *const *map, const char *const enum_type_name,
    const char *const str_value, int32_t default_value)
{
    const char *const key =
        String_FormatStatic("%s|%s", enum_type_name, str_value);
    M_STR_TO_ID_ENTRY *entry;
    HASH_FIND_STR(*map, key, entry);
    return entry != nullptr ? entry->value : default_value;
}

static const char *M_Id2Str(
    M_ID_TO_STR_ENTRY *const *map, const char *const enum_type_name,
    const int32_t enum_value)
{
    const char *const key =
        String_FormatStatic("%s|%d", enum_type_name, enum_value);
    M_ID_TO_STR_ENTRY *entry;
    HASH_FIND_STR(*map, key, entry);
    return entry != nullptr ? entry->str_value : nullptr;
}

void EnumMap_Define(
    const char *const enum_type_name, const char *const enum_name,
    const char *const label_key, const int32_t enum_value,
    const char *const str_value)
{
    M_DefineStr2Id(&m_Str2IdMap, enum_type_name, enum_value, str_value);
    M_DefineId2Str(&m_Id2StrMap, enum_type_name, enum_value, str_value);
    M_DefineId2Str(&m_Id2NameMap, enum_type_name, enum_value, enum_name);
    M_DefineId2Str(&m_Id2LabelKeyMap, enum_type_name, enum_value, label_key);
}

int32_t EnumMap_Get(
    const char *const enum_type_name, const char *const str_value,
    int32_t default_value)
{
    return M_Str2Id(&m_Str2IdMap, enum_type_name, str_value, default_value);
}

const char *EnumMap_ToString(
    const char *const enum_type_name, const int32_t enum_value)
{
    return M_Id2Str(&m_Id2StrMap, enum_type_name, enum_value);
}

const char *EnumMap_GetName(
    const char *const enum_type_name, const int32_t enum_value)
{
    return M_Id2Str(&m_Id2NameMap, enum_type_name, enum_value);
}

const char *EnumMap_GetLabel(
    const char *const enum_type_name, const int32_t enum_value)
{
    const char *const key =
        M_Id2Str(&m_Id2LabelKeyMap, enum_type_name, enum_value);
    if (key == nullptr) {
        return nullptr;
    }
    return GameString_Get(key);
}

VECTOR *EnumMap_ListValues(const char *const enum_type_name)
{
    if (enum_type_name == nullptr) {
        return nullptr;
    }

    // Compare the prefix to find the matching enum values.
    const size_t prefix_len = strlen(enum_type_name) + 1;

    VECTOR *const results = Vector_Create(sizeof(char *));
    M_ID_TO_STR_ENTRY *entry;
    M_ID_TO_STR_ENTRY *tmp;
    HASH_ITER(hh, m_Id2StrMap, entry, tmp)
    {
        if (strncmp(entry->key, enum_type_name, prefix_len - 1) == 0
            && entry->key[prefix_len - 1] == '|') {
            Vector_Add(results, &entry->str_value);
        }
    }
    return results;
}
