#include <trx/game/game_strings/manager.h>

#include <trx/core/filesystem.h>
#include <trx/core/json.h>
#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/core/utils.h>
#include <trx/core/vector.h>
#include <trx/debug.h>
#include <trx/game/game_flow/common.h>
#include <trx/game/game_strings/table.h>

#include <string.h>

typedef struct {
    char *path;
    bool load_levels;
} M_FILE_ENTRY;

typedef struct {
    char *lang;
    VECTOR *files;
    char *display_name;
    char *extends;
} M_LANG_ENTRY;

static VECTOR *m_SourceFiles = nullptr;
static VECTOR *m_LangEntries = nullptr;
static EVENT_MANAGER *m_EventManager = nullptr;

static void M_ClearFileEntries(VECTOR *const files)
{
    for (int32_t i = 0; i < files->count; i++) {
        const M_FILE_ENTRY *const file_entry = Vector_Get(files, i);
        Memory_Free(file_entry->path);
    }
    Vector_Free(files);
}

static void M_ClearLanguageEntries(void)
{
    if (m_LangEntries != nullptr) {
        for (int32_t i = 0; i < m_LangEntries->count; i++) {
            const M_LANG_ENTRY *const lang_entry = Vector_Get(m_LangEntries, i);
            Memory_Free(lang_entry->lang);
            Memory_Free(lang_entry->display_name);
            Memory_Free(lang_entry->extends);
            M_ClearFileEntries(lang_entry->files);
        }
        Vector_Free(m_LangEntries);
        m_LangEntries = nullptr;
    }
}

static void M_ClearManager(void)
{
    M_ClearLanguageEntries();
    if (m_SourceFiles != nullptr) {
        M_ClearFileEntries(m_SourceFiles);
        m_SourceFiles = nullptr;
    }
}

static M_LANG_ENTRY *M_FindLangEntry(const char *const lang)
{
    for (int32_t i = 0; i < m_LangEntries->count; i++) {
        M_LANG_ENTRY *const lang_entry = Vector_Get(m_LangEntries, i);
        if (String_Equivalent(lang_entry->lang, lang)) {
            return lang_entry;
        }
    }
    return nullptr;
}

// Create a new entry with the given file for the given language.
static void M_AddPathForLang(
    const char *const lang, char *const path, const bool load_levels)
{
    const M_LANG_ENTRY *lang_entry = M_FindLangEntry(lang);
    if (lang_entry == nullptr) {
        M_LANG_ENTRY new_ent = {
            .lang = Memory_DupStr(lang),
            .files = Vector_Create(sizeof(M_FILE_ENTRY)),
        };
        Vector_Add(m_LangEntries, &new_ent);
        lang_entry = M_FindLangEntry(lang);
    }
    const M_FILE_ENTRY file_entry = {
        .path = path,
        .load_levels = load_levels,
    };
    Vector_Add(lang_entry->files, &file_entry);
}

static void M_LoadLanguageNames(void)
{
    if (m_LangEntries == nullptr) {
        return;
    }
    for (int32_t i = 0; i < m_LangEntries->count; i++) {
        M_LANG_ENTRY *const lang_entry = Vector_Get(m_LangEntries, i);
        Memory_FreePointer(&lang_entry->display_name);
        Memory_FreePointer(&lang_entry->extends);
        if (lang_entry->files->count <= 0) {
            continue;
        }
        const M_FILE_ENTRY *const file_entry = Vector_Get(lang_entry->files, 0);
        char *data = nullptr;
        size_t size = 0;
        if (!File_Load(file_entry->path, &data, &size) || data == nullptr) {
            continue;
        }
        JSON_PARSE_RESULT pr = { 0 };
        JSON_VALUE *const root = JSON_ParseEx(
            data, size, JSON_PARSE_FLAGS_ALLOW_JSON5, nullptr, nullptr, &pr);
        if (root != nullptr) {
            JSON_OBJECT *const obj = JSON_ValueAsObject(root);
            const char *const name =
                JSON_ObjectGetString(obj, "language_name", JSON_INVALID_STRING);
            if (name != JSON_INVALID_STRING) {
                lang_entry->display_name = Memory_DupStr(name);
            }
            const char *const ext =
                JSON_ObjectGetString(obj, "extends", JSON_INVALID_STRING);
            if (ext != JSON_INVALID_STRING) {
                lang_entry->extends = Memory_DupStr(ext);
            }
            JSON_ValueFree(root);
        } else {
            LOG_WARNING(
                "failed to parse 'language_name' in %s: %s", file_entry->path,
                JSON_GetErrorDescription(pr.error));
        }
        Memory_Free(data);
    }
}

static void M_ReorderLanguages(void)
{
    if (m_LangEntries->count > 1) {
        VECTOR *ordered = Vector_Create(sizeof(M_LANG_ENTRY));
        const M_LANG_ENTRY *en_orig = M_FindLangEntry("en");
        if (en_orig != nullptr) {
            M_LANG_ENTRY en_entry = *en_orig;
            Vector_Add(ordered, &en_entry);
        }
        VECTOR *others = Vector_Create(sizeof(M_LANG_ENTRY));
        for (int32_t i = 0; i < m_LangEntries->count; ++i) {
            const M_LANG_ENTRY *entry = Vector_Get(m_LangEntries, i);
            if (en_orig != nullptr && String_Equivalent(entry->lang, "en")) {
                continue;
            }
            M_LANG_ENTRY e = *entry;
            Vector_Add(others, &e);
        }
        for (int32_t i = 0; i + 1 < others->count; ++i) {
            for (int32_t j = 0; j + 1 < others->count - i; ++j) {
                M_LANG_ENTRY *a = Vector_Get(others, j);
                M_LANG_ENTRY *b = Vector_Get(others, j + 1);
                const char *an = a->display_name ? a->display_name : "";
                const char *bn = b->display_name ? b->display_name : "";
                if (strcmp(an, bn) > 0) {
                    Vector_Swap(others, j, j + 1);
                }
            }
        }
        for (int32_t i = 0; i < others->count; ++i) {
            M_LANG_ENTRY *entry = Vector_Get(others, i);
            Vector_Add(ordered, entry);
        }
        Vector_Free(others);
        Vector_Free(m_LangEntries);
        m_LangEntries = ordered;
    }
}

void GameStringManager_Init(void)
{
    m_EventManager = EventManager_Create();
    M_ClearManager();
    m_SourceFiles = Vector_Create(sizeof(M_FILE_ENTRY));
}

void GameStringManager_Shutdown(void)
{
    if (m_EventManager != nullptr) {
        EventManager_Free(m_EventManager);
        m_EventManager = nullptr;
    }
    GameStringTable_Shutdown();
    M_ClearManager();
}

// Clear all previously set source strings files.
// Must be called before GameStringManager_AddSourceFile.
void GameStringManager_ClearSourceFiles(void)
{
    M_ClearManager();
    m_SourceFiles = Vector_Create(sizeof(M_FILE_ENTRY));
}

// Add a source strings file for language discovery and loading.
// base_path: path to a base strings JSON5 file.
// load_levels: true to load level entries from this source; false otherwise.
void GameStringManager_AddSourceFile(
    const char *const base_path, const bool load_levels)
{
    ASSERT(m_SourceFiles != nullptr);
    if (base_path == nullptr) {
        return;
    }
    const M_FILE_ENTRY fe = {
        .path = Memory_DupStr(base_path),
        .load_levels = load_levels,
    };
    Vector_Add(m_SourceFiles, &fe);
}

void GameStringManager_DiscoverLanguages(void)
{
    if (m_SourceFiles == nullptr) {
        return;
    }
    M_ClearLanguageEntries();
    m_LangEntries = Vector_Create(sizeof(M_LANG_ENTRY));

    for (int32_t i = 0; i < m_SourceFiles->count; ++i) {
        const M_FILE_ENTRY *src = Vector_Get(m_SourceFiles, i);
        char *dir = File_GetParentDirectory(src->path);
        const char *base =
            MAX(strrchr(src->path, '\\'), strrchr(src->path, '/'));
        base = (base != nullptr) ? base + 1 : src->path;
        const char *ext = strrchr(base, '.');
        if (dir == nullptr || ext == nullptr) {
            Memory_Free(dir);
            continue;
        }
        size_t stem_len = (size_t)(ext - base);
        size_t ext_len = strlen(ext);

        void *dh = File_OpenDirectory(dir);
        if (dh != nullptr) {
            const char *ent;
            while ((ent = File_ReadDirectory(dh))) {
                if (ent[0] == '.') {
                    continue;
                }
                size_t name_len = strlen(ent);
                if (name_len < stem_len + ext_len) {
                    continue;
                }
                if (strncmp(ent, base, stem_len) != 0
                    || strcmp(ent + name_len - ext_len, ext) != 0) {
                    continue;
                }
                char *code;
                if (name_len == stem_len + ext_len) {
                    code = Memory_DupStr("en");
                } else if (ent[stem_len] == '-') {
                    size_t code_len = name_len - stem_len - ext_len - 1;
                    code = String_Format(
                        "%.*s", (int32_t)code_len, ent + stem_len + 1);
                } else {
                    continue;
                }
                char *path = String_Format("%s/%s", dir, ent);
                M_AddPathForLang(code, path, src->load_levels);
                Memory_Free(code);
            }
            File_CloseDirectory(dh);
        }
        Memory_Free(dir);
    }

    M_LoadLanguageNames();
    M_ReorderLanguages();
}

VECTOR *GameStringManager_GetAvailableLanguages(void)
{
    if (m_LangEntries == nullptr) {
        return nullptr;
    }
    VECTOR *const out = Vector_Create(sizeof(char *));
    for (int32_t i = 0; i < m_LangEntries->count; i++) {
        const M_LANG_ENTRY *const lang_entry = Vector_Get(m_LangEntries, i);
        char *const c = Memory_DupStr(lang_entry->lang);
        Vector_Add(out, &c);
    }
    return out;
}

// Recursive load of language chain (handles 'extends' fallback between
// dialects)
static bool M_ReloadLangRec(const char *const lang, VECTOR *const visited)
{
    for (int32_t i = 0; i < visited->count; i++) {
        const char *const prev = *(char **)Vector_Get(visited, i);
        if (String_Equivalent(prev, lang)) {
            LOG_WARNING("cyclic language extends detected: %s", lang);
            return false;
        }
    }
    Vector_Add(visited, &lang);
    M_LANG_ENTRY *const entry = M_FindLangEntry(lang);
    if (entry == nullptr) {
        return false;
    }
    if (entry->extends) {
        if (!M_ReloadLangRec(entry->extends, visited)) {
            return false;
        }
    }
    for (int32_t i = 0; i < entry->files->count; i++) {
        const M_FILE_ENTRY *const fe = Vector_Get(entry->files, i);
        if (!GameStringTable_Load(fe->path, fe->load_levels)) {
            return false;
        }
    }
    return true;
}

bool GameStringManager_ReloadLanguage(const char *lang)
{
    const M_LANG_ENTRY *const base_entry =
        m_LangEntries ? M_FindLangEntry(lang) : nullptr;
    if (base_entry == nullptr) {
        LOG_WARNING("language '%s' not found, defaulting to base", lang);
        lang = "en";
    }
    GameStringTable_Shutdown();
    GameStringTable_Init();
    VECTOR *const visited = Vector_Create(sizeof(char *));
    const bool success = M_ReloadLangRec(lang, visited);
    Vector_Free(visited);
    if (success) {
        GameStringTable_Apply(GF_GetCurrentLevel());
        if (m_EventManager != nullptr) {
            const EVENT event = {
                .name = "reload_language",
                .sender = nullptr,
                .data = (void *)lang,
            };
            EventManager_Fire(m_EventManager, &event);
        }
    }
    return success;
}

const char *GameStringManager_GetLanguageName(const char *const code)
{
    if (m_LangEntries == nullptr || code == nullptr) {
        return nullptr;
    }
    const M_LANG_ENTRY *const ent = M_FindLangEntry(code);
    return ent != nullptr ? ent->display_name : nullptr;
}

int32_t GameStringManager_SubscribeReload(
    const EVENT_LISTENER listener, void *const user_data)
{
    ASSERT(m_EventManager != nullptr);
    return EventManager_Subscribe(
        m_EventManager, "reload_language", nullptr, listener, user_data);
}

void GameStringManager_UnsubscribeReload(const int32_t listener_id)
{
    if (m_EventManager != nullptr) {
        EventManager_Unsubscribe(m_EventManager, listener_id);
    }
}
