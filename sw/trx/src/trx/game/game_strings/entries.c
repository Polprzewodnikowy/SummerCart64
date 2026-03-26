#include <trx/game/game_strings/entries.h>

#include <trx/core/memory.h>

#include <uthash.h>

// One-slot-per-string for stable indirection on reload.
typedef struct M_SLOT {
    const char *value;
} M_SLOT;

typedef struct {
    char *key;
    M_SLOT *slot;
    UT_hash_handle hh;
} M_STRING_ENTRY;

static M_STRING_ENTRY *m_StringTable = nullptr;

void GameString_Init(void)
{
#include <trx/game/game_strings/entries.def>
}

void GameString_Shutdown(void)
{
    GameString_Clear();
}

void GameString_Define(const char *const key, const char *value)
{
    M_STRING_ENTRY *entry;

    HASH_FIND_STR(m_StringTable, key, entry);
    if (entry == nullptr) {
        entry = Memory_Alloc(sizeof(*entry));
        entry->key = Memory_DupStr(key);
        entry->slot = Memory_Alloc(sizeof(*entry->slot));
        entry->slot->value = nullptr;
        HASH_ADD_KEYPTR(
            hh, m_StringTable, entry->key, strlen(entry->key), entry);
    }
    Memory_Free((void *)entry->slot->value);
    entry->slot->value = Memory_DupStr(value);
}

bool GameString_IsKnown(const char *const key)
{
    M_STRING_ENTRY *entry;
    HASH_FIND_STR(m_StringTable, key, entry);
    return entry != nullptr;
}

const char *GameString_Get(const char *const key)
{
    M_STRING_ENTRY *entry;
    HASH_FIND_STR(m_StringTable, key, entry);
    return entry ? entry->slot->value : nullptr;
}

const char *const *GameString_GetPtr(const char *const key)
{
    M_STRING_ENTRY *entry;
    HASH_FIND_STR(m_StringTable, key, entry);
    return entry ? &entry->slot->value : nullptr;
}

void GameString_Clear(void)
{
    M_STRING_ENTRY *entry, *tmp;

    HASH_ITER(hh, m_StringTable, entry, tmp)
    {
        HASH_DEL(m_StringTable, entry);
        Memory_Free(entry->key);
        Memory_Free((void *)entry->slot->value);
        Memory_Free(entry->slot);
        Memory_Free(entry);
    }
}
