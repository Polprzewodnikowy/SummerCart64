#include <trx/game/level/cache.h>

#include <trx/core/filesystem.h>
#include <trx/core/hash.h>
#include <trx/core/json/util/file.h>
#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/game/shell.h>

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <uthash.h>

#define M_CACHE_MAGIC UINT32_C(0x4C434831)

typedef struct {
    uint32_t magic;
    uint64_t checksum;
} M_CACHE_HEADER;

typedef struct M_LEVEL_HASH_ENTRY {
    const GF_LEVEL *level;
    uint64_t hash;
    UT_hash_handle hh;
} M_LEVEL_HASH_ENTRY;

static M_LEVEL_HASH_ENTRY *m_LevelHashMap = nullptr;

static void M_ClearLevelHashMap(void)
{
    M_LEVEL_HASH_ENTRY *current = nullptr;
    M_LEVEL_HASH_ENTRY *tmp = nullptr;
    HASH_ITER(hh, m_LevelHashMap, current, tmp)
    {
        HASH_DEL(m_LevelHashMap, current);
        Memory_FreePointer(&current);
    }
}

static __attribute__((constructor)) void M_Initialise(void)
{
    m_LevelHashMap = nullptr;
}

static __attribute__((destructor)) void M_Shutdown(void)
{
    M_ClearLevelHashMap();
}

static void M_GetFileMeta(
    const char *const path, uint64_t *const out_size, uint64_t *const out_mtime)
{
    uint64_t size = 0;
    uint64_t mtime = 0;
    File_GetMeta(path, &size, &mtime);

    if (out_size != nullptr) {
        *out_size = size;
    }
    if (out_mtime != nullptr) {
        *out_mtime = mtime;
    }
}

static uint64_t M_ComputeLevelHash(const GF_LEVEL *const level)
{
    uint64_t checksum = Hash_FNV1a64_Init();
    checksum = Hash_FNV1a64_UpdateU32(checksum, (uint32_t)level->num);
    checksum = Hash_FNV1a64_UpdateU32(checksum, (uint32_t)level->type);
    checksum = Hash_FNV1a64_UpdateString(checksum, level->path);
    checksum =
        Hash_FNV1a64_UpdateU32(checksum, (uint32_t)level->injections.count);

    if (level->path != nullptr) {
        uint64_t file_size = 0;
        uint64_t file_mtime = 0;
        M_GetFileMeta(level->path, &file_size, &file_mtime);
        checksum = Hash_FNV1a64_UpdateU64(checksum, file_size);
        checksum = Hash_FNV1a64_UpdateU64(checksum, file_mtime);
    }

    for (int32_t i = 0; i < level->injections.count; i++) {
        const char *const path = level->injections.data_paths[i];
        uint64_t file_size = 0;
        uint64_t file_mtime = 0;
        checksum = Hash_FNV1a64_UpdateString(checksum, path);
        if (path != nullptr) {
            M_GetFileMeta(path, &file_size, &file_mtime);
            checksum = Hash_FNV1a64_UpdateU64(checksum, file_size);
            checksum = Hash_FNV1a64_UpdateU64(checksum, file_mtime);
        }
    }

    return checksum;
}

static uint64_t M_GetLevelHash(const GF_LEVEL *const level)
{
    M_LEVEL_HASH_ENTRY *entry = nullptr;
    HASH_FIND_PTR(m_LevelHashMap, &level, entry);
    if (entry != nullptr) {
        return entry->hash;
    }

    entry = Memory_Alloc(sizeof(*entry));
    entry->level = level;
    entry->hash = M_ComputeLevelHash(level);
    HASH_ADD_PTR(m_LevelHashMap, level, entry);
    return entry->hash;
}

uint64_t LevelCache_InitChecksum(
    const char *const scope, const uint32_t version)
{
    uint64_t checksum = Hash_FNV1a64_Init();
    checksum = Hash_FNV1a64_UpdateString(checksum, scope);
    checksum = Hash_FNV1a64_UpdateU32(checksum, version);
    return checksum;
}

uint64_t LevelCache_UpdateLevelChecksum(
    uint64_t checksum, const GF_LEVEL *const level)
{
    if (level == nullptr) {
        return checksum;
    }

    checksum = Hash_FNV1a64_UpdateU64(checksum, M_GetLevelHash(level));
    return checksum;
}

const char *LevelCache_GetLevelKey(const GF_LEVEL *const level)
{
    if (level == nullptr) {
        return nullptr;
    }

    char type_key = 'u';
    switch (level->type) {
    case GFL_TITLE:
        type_key = 't';
        break;
    case GFL_NORMAL:
        type_key = 'l';
        break;
    case GFL_CUTSCENE:
        type_key = 'c';
        break;
    case GFL_DEMO:
        type_key = 'd';
        break;
    case GFL_GYM:
        type_key = 'g';
        break;
    case GFL_BONUS:
        type_key = 'b';
        break;
    case GFL_DUMMY:
        type_key = 'x';
        break;
    case GFL_CURRENT:
        type_key = 'r';
        break;
    }

    const char *name = level->path != nullptr ? level->path : "unknown";
    const char *const slash = strrchr(name, '/');
    const char *const backslash = strrchr(name, '\\');
    if (slash != nullptr && backslash != nullptr) {
        name = slash > backslash ? slash + 1 : backslash + 1;
    } else if (slash != nullptr) {
        name = slash + 1;
    } else if (backslash != nullptr) {
        name = backslash + 1;
    }

    const size_t stem_len = strcspn(name, ".");
    return String_FormatStatic(
        "%c%d_%.*s", type_key, level->num, (int)stem_len, name);
}

static const char *M_GetPath(const char *const filename)
{
    const SHELL_ARGS *const args = Shell_GetArgs();
    if (args == nullptr || args->mod == nullptr || args->mod->name == nullptr) {
        return nullptr;
    }

    return String_FormatStatic(
        "%s/%s/%s", Shell_GetCacheDir(), args->mod->name, filename);
}

MYFILE *LevelCache_OpenBinaryRead(
    const char *const filename, const uint64_t checksum)
{
    const char *const path = M_GetPath(filename);
    if (path == nullptr) {
        return nullptr;
    }

    MYFILE *const file = File_Open(path, FILE_OPEN_READ);
    if (file == nullptr) {
        return nullptr;
    }

    M_CACHE_HEADER header;
    if (!File_ReadData(file, &header, sizeof(header))
        || header.magic != M_CACHE_MAGIC || header.checksum != checksum) {
        File_Close(file);
        return nullptr;
    }

    return file;
}

MYFILE *LevelCache_OpenBinaryWrite(
    const char *const filename, const uint64_t checksum)
{
    const char *const path = M_GetPath(filename);
    if (path == nullptr) {
        return nullptr;
    }

    File_EnsureParentDirectories(path);

    MYFILE *const file = File_Open(path, FILE_OPEN_WRITE);
    if (file == nullptr) {
        return nullptr;
    }

    const M_CACHE_HEADER header = {
        .magic = M_CACHE_MAGIC,
        .checksum = checksum,
    };
    File_WriteData(file, &header, sizeof(header));

    return file;
}

JSON_VALUE *LevelCache_ReadJSON(
    const char *const filename, const uint64_t checksum)
{
    const char *const path = M_GetPath(filename);
    if (path == nullptr) {
        return nullptr;
    }

    JSON_VALUE *const root = JSONFile_Read(path);
    if (root == nullptr) {
        return nullptr;
    }

    JSON_OBJECT *const root_obj = JSON_ValueAsObject(root);
    const char *const checksum_str = root_obj != nullptr
        ? JSON_ObjectGetString(root_obj, "checksum", nullptr)
        : nullptr;
    if (checksum_str == nullptr
        || (uint64_t)strtoull(checksum_str, nullptr, 16) != checksum) {
        JSON_ValueFree(root);
        return nullptr;
    }

    return root;
}

bool LevelCache_WriteJSON(
    const char *const filename, const uint64_t checksum, JSON_VALUE *const root)
{
    const char *const path = M_GetPath(filename);
    JSON_OBJECT *const root_obj =
        root != nullptr ? JSON_ValueAsObject(root) : nullptr;
    if (path == nullptr || root_obj == nullptr) {
        return false;
    }

    File_EnsureParentDirectories(path);

    JSON_ObjectAppendString(
        root_obj, "checksum", String_FormatStatic("%016" PRIx64, checksum));

    return JSONFile_Write(path, root);
}
