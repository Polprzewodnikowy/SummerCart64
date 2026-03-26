#include <trx/game/catalog/manager.h>

#include <trx/core/filesystem.h>
#include <trx/core/log.h>
#include <trx/core/memory.h>
#include <trx/game/items/actions/ids.h>
#include <trx/game/lara/enum.h>
#include <trx/game/music/ids.h>
#include <trx/game/objects/ids.h>
#include <trx/game/sound/ids.h>

#include <ctype.h>
#include <string.h>
#include <uthash.h>

// Compile-time table of catalog IDs and their name strings
typedef struct {
    CATALOG_CONTEXT context;
    CATALOG_ID id;
    const char *name_str;
} M_ENTRY;

static const M_ENTRY m_CatalogEntryDefs[] = {
#define X_CATALOG_ID(enum_value) { CATALOG_MUSIC, enum_value, #enum_value },
#include <trx/game/catalog/music.def>
#undef X_CATALOG_ID
#define X_CATALOG_ID(enum_value) { CATALOG_OBJECTS, enum_value, #enum_value },
#include <trx/game/catalog/objects.def>
#undef X_CATALOG_ID
#define X_CATALOG_ID(enum_value) { CATALOG_SAMPLES, enum_value, #enum_value },
#include <trx/game/catalog/samples.def>
#undef X_CATALOG_ID
#define X_CATALOG_ID(enum_value)                                               \
    { CATALOG_LARA_STATES, enum_value, #enum_value },
#include <trx/game/catalog/lara_states.def>
#undef X_CATALOG_ID
#define X_CATALOG_ID(enum_value)                                               \
    { CATALOG_LARA_ANIMS, enum_value, #enum_value },
#include <trx/game/catalog/lara_anims.def>
#undef X_CATALOG_ID
#define X_CATALOG_ID(enum_value)                                               \
    { CATALOG_ITEM_ACTIONS, enum_value, #enum_value },
#include <trx/game/catalog/item_actions.def>
#undef X_CATALOG_ID
};

// Number of catalog entries
static const size_t m_CatalogEntryCount =
    sizeof(m_CatalogEntryDefs) / sizeof(m_CatalogEntryDefs[0]);

// Internal map from name to CATALOG_ID
typedef struct {
    const char *name_str;
    int32_t enum_value;
    UT_hash_handle hh;
} M_NAME_ENTRY;
static M_NAME_ENTRY *m_Name2EnumMap[CATALOG_CONTEXT_MAX] = { nullptr };

// Internal map from game ID to CATALOG_ID
typedef struct {
    int32_t game_id;
    int32_t enum_value;
    UT_hash_handle hh;
} M_GAME_ID_ENTRY;
static M_GAME_ID_ENTRY *m_GameID2EnumMap[CATALOG_CONTEXT_MAX] = { nullptr };

// Parsed game IDs arrays (dynamically sized)
static int32_t **m_CatalogGameIDs = nullptr;

// State flag
static bool m_Initialized = false;

// Helper: clear game_id->enum map
static void M_ClearGameIDMap(M_GAME_ID_ENTRY **const map)
{
    M_GAME_ID_ENTRY *cur, *tmp;
    HASH_ITER(hh, *map, cur, tmp)
    {
        HASH_DEL(*map, cur);
        Memory_Free(cur);
    }
}

// Helper: clear name->enum map
static void M_ClearNameMap(M_NAME_ENTRY **const map)
{
    M_NAME_ENTRY *cur, *tmp;
    HASH_ITER(hh, *map, cur, tmp)
    {
        HASH_DEL(*map, cur);
        Memory_Free(cur);
    }
}

// Trim leading/trailing whitespace in-place
static char *M_StrTrim(char *s)
{
    while (*s && isspace(*s)) {
        s++;
    }
    if (*s == '\0') {
        return s;
    }
    char *end = s + strlen(s) - 1;
    while (end > s && isspace(*end)) {
        *end-- = '\0';
    }
    return s;
}

// Parse one CSV field into out buffer; advance *p past field
static void M_ParseCSVField(
    const char **const p, char *const out, const size_t out_sz)
{
    const char *src = *p;
    char *dst = out;
    const char *const end = out + out_sz - 1;
    if (*src == '"') {
        src++;
        while (*src && (*src != '"' || (src[1] == '"'))) {
            if (*src == '"' && src[1] == '"') {
                src++;
            }
            if (dst < end) {
                *dst++ = *src;
            }
            src++;
        }
        if (*src == '"') {
            src++;
        }
    } else {
        while (*src && *src != ',') {
            if (dst < end) {
                *dst++ = *src;
            }
            src++;
        }
    }
    *dst = '\0';
    if (*src == ',') {
        src++;
    }
    *p = src;
}

// Build initial maps on first load
static void M_Initialize(void)
{
    const size_t count = m_CatalogEntryCount;
    m_CatalogGameIDs =
        Memory_Alloc(sizeof(*m_CatalogGameIDs) * CATALOG_CONTEXT_MAX);
    for (size_t ctx = 0; ctx < CATALOG_CONTEXT_MAX; ctx++) {
        m_CatalogGameIDs[ctx] =
            Memory_Alloc(sizeof(*m_CatalogGameIDs[ctx]) * count);
    }
    for (size_t idx = 0; idx < count; idx++) {
        const CATALOG_CONTEXT ctx = m_CatalogEntryDefs[idx].context;
        const CATALOG_ID id = m_CatalogEntryDefs[idx].id;
        m_CatalogGameIDs[ctx][id] = -1;
        M_NAME_ENTRY *const entry = Memory_Alloc(sizeof(*entry));
        entry->name_str = m_CatalogEntryDefs[idx].name_str;
        entry->enum_value = id;
        HASH_ADD_KEYPTR(
            hh, m_Name2EnumMap[ctx], entry->name_str,
            (uint32_t)strlen(entry->name_str), entry);
    }
    m_Initialized = true;
}

bool Catalog_Load(
    const CATALOG_CONTEXT context, const char *const csv_path,
    const bool allow_duplicates)
{
    if (!m_Initialized) {
        M_Initialize();
    }
    char *file_data;
    size_t file_size;
    if (!File_Load(csv_path, &file_data, &file_size)) {
        return false;
    }

    const char *pos = file_data;
    const char *end = file_data + file_size;
    char line[512];
    while (pos < end) {
        size_t len = 0;
        while (pos < end && *pos != '\n' && len + 1 < sizeof(line)) {
            line[len++] = *pos++;
        }
        if (pos < end && *pos == '\n') {
            pos++;
        }
        line[len] = '\0';
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }
        const char *p = line;
        char id_buf[32];
        char name_buf[64];
        M_ParseCSVField(&p, id_buf, sizeof(id_buf));
        M_ParseCSVField(&p, name_buf, sizeof(name_buf));
        char *const id_str = M_StrTrim(id_buf);
        char *const name_str = M_StrTrim(name_buf);
        const int32_t game_id = (int32_t)strtol(id_str, nullptr, 10);

        CATALOG_ID id;
        if (!Catalog_NameToEnum(context, name_str, &id)) {
            continue;
        }

        m_CatalogGameIDs[context][id] = game_id;
        if (game_id >= 0) {
            M_GAME_ID_ENTRY *existing = nullptr;
            HASH_FIND_INT(m_GameID2EnumMap[context], &game_id, existing);
            if (existing == nullptr) {
                M_GAME_ID_ENTRY *gentry = Memory_Alloc(sizeof(*gentry));
                gentry->game_id = game_id;
                gentry->enum_value = id;
                HASH_ADD_INT(m_GameID2EnumMap[context], game_id, gentry);
            } else if (!allow_duplicates) {
                LOG_ERROR(
                    "Duplicate game ID %d for context %d", game_id, context);
            }
        }
    }
    Memory_FreePointer(&file_data);
    return true;
}

bool Catalog_NameToEnum(
    const CATALOG_CONTEXT context, const char *name, CATALOG_ID *const out_id)
{
    M_NAME_ENTRY *entry = nullptr;
    HASH_FIND_STR(m_Name2EnumMap[context], name, entry);
    if (entry != nullptr) {
        *out_id = (CATALOG_ID)entry->enum_value;
        return true;
    }
    return false;
}

bool Catalog_EnumToGameID(
    const CATALOG_CONTEXT context, const CATALOG_ID id,
    int32_t *const out_game_id)
{
    if (id < 0 || (size_t)id >= m_CatalogEntryCount) {
        return false;
    }
    const int32_t gid = m_CatalogGameIDs[context][id];
    if (gid < 0) {
        return false;
    }
    *out_game_id = gid;
    return true;
}

bool Catalog_GameIDToEnum(
    const CATALOG_CONTEXT context, const int32_t game_id,
    CATALOG_ID *const out_id)
{
    M_GAME_ID_ENTRY *entry = nullptr;
    HASH_FIND_INT(m_GameID2EnumMap[context], &game_id, entry);
    if (entry != nullptr) {
        *out_id = (CATALOG_ID)entry->enum_value;
        return true;
    }
    return false;
}

void Catalog_Shutdown(void)
{
    if (!m_Initialized) {
        return;
    }
    for (size_t ctx = 0; ctx < CATALOG_CONTEXT_MAX; ctx++) {
        M_ClearGameIDMap(&m_GameID2EnumMap[ctx]);
        M_ClearNameMap(&m_Name2EnumMap[ctx]);
        Memory_Free(m_CatalogGameIDs[ctx]);
    }
    Memory_Free(m_CatalogGameIDs);
    m_CatalogGameIDs = nullptr;
    m_Initialized = false;
}
