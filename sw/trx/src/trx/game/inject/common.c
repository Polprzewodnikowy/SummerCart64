#include <trx/game/inject/common.h>

#include <trx/config.h>
#include <trx/core/benchmark.h>
#include <trx/core/memory.h>
#include <trx/core/thread_pool.h>
#include <trx/core/vector.h>
#include <trx/debug.h>
#include <trx/game/items.h>
#include <trx/game/level.h>
#include <trx/game/rooms.h>
#include <trx/game/savegame.h>
#include <trx/version.h>

#include <string.h>
#include <zlib.h>

#define M_INJECTION_CURRENT_VERSION 6
#define M_VIRTUAL_NAME "virtual_injection"

typedef struct {
    INJECTION *injection;
    const char *path;
} M_LOAD_JOB;

static bool (*m_Testers[ITT_NUMBER_OF])(
    const INJECTION_CONTEXT *, const INJECTION *injection) = {};
static void (*m_Handlers[ICT_NUMBER_OF])(
    const INJECTION_CONTEXT *, INJECTION_CHUNK chunk) = {};

static INJECTION_CONTEXT m_Context = {};
static int32_t m_NumInjections = 0;
static INJECTION *m_Injections = nullptr;

static int32_t m_DataCounts[IDT_NUMBER_OF] = {};
static int32_t m_MaxStaticObject3DId = -1;
static int32_t m_MaxStaticObject2DId = -1;
static VECTOR *m_RoomMeta = nullptr;
static LEVEL_CONTEXT_INFO m_CachedInfo = {};
static uint16_t *m_PaletteMap = nullptr;
static size_t m_PaletteMapSize = 0;

static bool M_IsRelevant(
    const INJECTION_CONTEXT *const ctx, const INJECTION_FILE_TYPE type)
{
    const bool stats = (ctx->mode == INJECTION_MODE_STATS);

    if (stats) {
        switch (type) {
        case IFT_GENERAL:
        case IFT_FLOOR_DATA:
        case IFT_PS1_ENEMY:
            break;

        default:
            return false;
        }
    }

    switch (type) {
    case IFT_GENERAL:
    case IFT_LARA_ANIMS:
    case IFT_BRAID:
    case IFT_SKYBOX:
        return true;

    case IFT_FLOOR_DATA:
        return g_Config.gameplay.fix_floor_data_issues;

    case IFT_ITEM_POSITION:
        return g_Config.visuals.fix_item_rots;

    case IFT_TEXTURE_FIX:
        return g_Config.visuals.fix_texture_issues;

    case IFT_ALTER_ANIM_SPRITE:
        return g_Config.visuals.fix_animated_sprites == (g_TRVersion >= 2);

    case IFT_PS1_CRYSTAL:
        return g_Config.gameplay.enable_save_crystals
            && g_Config.visuals.enable_ps1_crystals;

    case IFT_PS1_SFX:
        return g_Config.audio.enable_ps1_sfx;

    case IFT_PS1_ENEMY: {
        if (!g_Config.gameplay.restore_ps1_enemies) {
            return false;
        }
        const SAVEGAME_INFO *const info =
            Savegame_GetSavegameInfo(Savegame_GetBoundSlot());
        if (info != nullptr && (info->initial_version == SG_VERSION_LEGACY)) {
            return false;
        }
        return true;
    }

    default:
        return false;
    }
}

static INJECTION_CHUNK M_ReadChunk(const INJECTION *const injection)
{
    return (INJECTION_CHUNK) {
        .injection = injection,
        .type = VFile_ReadS32(injection->fp),
        .num_blocks = VFile_ReadS32(injection->fp),
        .total_size = VFile_ReadS32(injection->fp),
    };
}

static void M_InitialiseBlock(
    VFILE *const file, const INJECTION_VERSION version)
{
    const INJECTION_DATA_TYPE data_type = VFile_ReadS32(file);
    const int32_t data_count = VFile_ReadS32(file);
    const int32_t data_size = VFile_ReadS32(file);
    if (data_type >= 0 && data_type < IDT_NUMBER_OF) {
        m_DataCounts[data_type] += data_count;
    }

    switch (data_type) {
    case IDT_STATIC_OBJECTS: {
        for (int32_t i = 0; i < data_count; i++) {
            const int32_t static_id = VFile_ReadS32(file);
            if (static_id > m_MaxStaticObject3DId) {
                m_MaxStaticObject3DId = static_id;
            }
            VFile_Skip(file, 28);
        }
        return;
    }

    case IDT_SPRITE_SEQUENCES: {
        for (int32_t i = 0; i < data_count; i++) {
            const INJECT_OBJECT_TYPE obj_type = VFile_ReadS32(file);
            const int32_t obj_id = VFile_ReadS32(file);
            if (obj_type == OBJ_TYPE_STATIC2D
                && obj_id > m_MaxStaticObject2DId) {
                m_MaxStaticObject2DId = obj_id;
            }
            if (obj_type == OBJ_TYPE_OBJECT && version < INJ_VERSION_5) {
                VFile_Skip(file, 16);
            }
            VFile_Skip(file, sizeof(int16_t) * 2);
        }
        return;
    }

    case IDT_ROOM_EDIT_META: {
        if (m_RoomMeta == nullptr) {
            m_RoomMeta = Vector_Create(sizeof(INJECTION_MESH_META));
        }
        for (int32_t i = 0; i < data_count; i++) {
            INJECTION_MESH_META meta = {
                .room_index = VFile_ReadS16(file),
                .num_vertices = VFile_ReadS16(file),
                .num_quads = VFile_ReadS16(file),
                .num_triangles = VFile_ReadS16(file),
                .num_static_2ds = VFile_ReadS16(file),
            };
            if (version >= INJ_VERSION_3) {
                meta.num_static_3ds = VFile_ReadS16(file);
            }
            Vector_Add(m_RoomMeta, &meta);
        }

        return;
    }

    case IDT_SAMPLE_INFOS: {
        for (int32_t i = 0; i < data_count; i++) {
            // Skip ID, volume and chance
            VFile_Skip(file, 3 * sizeof(int16_t));
            const int16_t flags = VFile_ReadS16(file);
            if (version >= INJ_VERSION_6) {
                // Skip range and pitch
                VFile_Skip(file, sizeof(int32_t) + sizeof(int8_t));
            }
            const int16_t num_samples = (flags >> 2) & 0xF;
            m_DataCounts[IDT_SAMPLE_INDICES] += num_samples;
            if (g_TRVersion == 1 || version >= INJ_VERSION_4) {
                for (int32_t j = 0; j < num_samples; j++) {
                    const int32_t sample_length = VFile_ReadS32(file);
                    m_DataCounts[IDT_SAMPLE_DATA] += sample_length;
                    VFile_Skip(file, sizeof(char) * sample_length);
                }
            } else if (g_TRVersion >= 2) {
                VFile_Skip(file, sizeof(uint32_t));
            }
        }

        return;
    }

    default:
        break;
    }

    VFile_Skip(file, data_size);
}

static void M_ReadVFile(
    INJECTION *const injection, VFILE *const file, const char *const file_name)
{
    const char *const inj_name =
        file_name == nullptr ? M_VIRTUAL_NAME : file_name;
    char *payload = nullptr;
    injection->path = Memory_DupStr(inj_name);

    const uint32_t magic = VFile_ReadU32(file);
    if (magic != INJECTION_MAGIC) {
        LOG_WARNING("Invalid injection magic in %s", inj_name);
        goto cleanup;
    }

    injection->version = VFile_ReadS32(file);
    if (injection->version < INJ_VERSION_2
        || injection->version > M_INJECTION_CURRENT_VERSION) {
        LOG_WARNING(
            "%s uses unsupported version %d", inj_name, injection->version);
        goto cleanup;
    }

    injection->type = VFile_ReadS32(file);
    if (injection->type < 0 || injection->type >= IFT_NUMBER_OF) {
        LOG_WARNING("%s is of unknown type %d", inj_name, injection->type);
        goto cleanup;
    }

    injection->relevant = M_IsRelevant(&m_Context, injection->type);
    if (!injection->relevant) {
        goto cleanup;
    }

    const int32_t uncompressed_size = VFile_ReadS32(file);
    const int32_t compressed_size = VFile_ReadS32(file);

    const char *compressed = file->cur_ptr;
    payload = Memory_Alloc(uncompressed_size);

    uLongf uncompressed_sizef = uncompressed_size;
    const int32_t error_code = uncompress(
        (Bytef *)payload, &uncompressed_sizef, (const Bytef *)compressed,
        (uLongf)compressed_size);
    if (error_code != Z_OK) {
        LOG_WARNING("Failed to decompress injection payload (%d)", error_code);
        injection->relevant = false;
        goto cleanup;
    }

    injection->fp = VFile_CreateFromBuffer(payload, uncompressed_size);
    if (m_Context.mode != INJECTION_MODE_STATS) {
        LOG_INFO("%s queued for injection", inj_name);
    }

cleanup:
    Memory_FreePointer(&payload);
    VFile_Close(file);
}

static void M_InitialiseInjection(INJECTION *const injection)
{
    if (!injection->relevant || injection->fp == nullptr) {
        return;
    }

    VFile_SetPos(injection->fp, 0);

    {
        // Tests are executed after the main level data is loaded.
        VFile_Skip(injection->fp, sizeof(int32_t));
        const int32_t test_size = VFile_ReadS32(injection->fp);
        VFile_Skip(injection->fp, test_size);
    }

    const int32_t num_chunks = VFile_ReadS32(injection->fp);
    for (int32_t i = 0; i < num_chunks; i++) {
        const INJECTION_CHUNK chunk = M_ReadChunk(injection);
        for (int32_t j = 0; j < chunk.num_blocks; j++) {
            M_InitialiseBlock(injection->fp, injection->version);
        }
    }

    VFile_SetPos(injection->fp, 0);
}

static void M_LoadFromFile(
    INJECTION *const injection, const char *const file_name)
{
    VFILE *const file = VFile_CreateFromPath(file_name);
    if (file == nullptr) {
        LOG_WARNING("Could not open %s", file_name);
        return;
    }

    M_ReadVFile(injection, file, file_name);
}

static void M_LoadInjectionJob(void *const user_data)
{
    const M_LOAD_JOB *const job = user_data;
    M_LoadFromFile(job->injection, job->path);
}

static bool M_IsApplicable(const INJECTION *const injection)
{
    const int32_t test_count = VFile_ReadS32(injection->fp);
    VFile_Skip(injection->fp, sizeof(int32_t));

    bool applicable = true;
    for (int32_t i = 0; i < test_count; i++) {
        const INJECTION_TEST_TYPE type = VFile_ReadS32(injection->fp);
        if (m_Testers[type] == nullptr) {
            LOG_WARNING("Unknown injection test type %d", type);
            applicable = false;
            break;
        } else {
            applicable &= m_Testers[type](&m_Context, injection);
        }
    }

    return applicable;
}

void Inject_RegisterTester(
    const INJECTION_TEST_TYPE type,
    bool (*test_func)(const INJECTION_CONTEXT *, const INJECTION *injection))
{
    m_Testers[type] = test_func;
}

void Inject_RegisterHandler(
    const INJECTION_CHUNK_TYPE type,
    void (*handle_func)(const INJECTION_CONTEXT *, INJECTION_CHUNK chunk))
{
    m_Handlers[type] = handle_func;
}

void Inject_RegisterPaletteMap(const uint16_t *palette_map, const int32_t size)
{
    Memory_FreePointer(&m_PaletteMap);
    m_PaletteMap = Memory_Alloc(size * sizeof(int16_t));
    m_PaletteMapSize = size;
    memcpy(m_PaletteMap, palette_map, size * sizeof(int16_t));
}

uint16_t Inject_GetPaletteIndex(const uint16_t index)
{
    ASSERT(index < m_PaletteMapSize);
    return m_PaletteMap == nullptr ? 0 : m_PaletteMap[index];
}

void Inject_InitLevel(const GF_LEVEL *const level, const INJECTION_MODE mode)
{
    m_Context.mode = mode;
    m_NumInjections = level->injections.count;
    if (m_NumInjections == 0) {
        return;
    }

    BENCHMARK benchmark = Benchmark_Start();

    m_Injections = Memory_Alloc(sizeof(INJECTION) * m_NumInjections);
    if (m_NumInjections > 1) {
        M_LOAD_JOB *const jobs =
            Memory_Alloc(sizeof(M_LOAD_JOB) * m_NumInjections);

        THREAD_POOL *const pool = ThreadPool_Create(-1);
        ASSERT(pool != nullptr);
        for (int32_t i = 0; i < m_NumInjections; i++) {
            jobs[i] = (M_LOAD_JOB) {
                .injection = &m_Injections[i],
                .path = level->injections.data_paths[i],
            };
            ThreadPool_AddJob(pool, M_LoadInjectionJob, &jobs[i]);
        }

        ThreadPool_Wait(pool);
        ThreadPool_Destroy(pool);

        Memory_Free(jobs);
    } else {
        M_LoadFromFile(&m_Injections[0], level->injections.data_paths[0]);
    }

    for (int32_t i = 0; i < m_NumInjections; i++) {
        M_InitialiseInjection(&m_Injections[i]);
    }

    if (m_Context.mode != INJECTION_MODE_STATS) {
        Benchmark_End(&benchmark, nullptr);
    }
}

void Inject_AppendInjection(VFILE *const file)
{
    m_Injections =
        Memory_Realloc(m_Injections, sizeof(INJECTION) * (m_NumInjections + 1));
    INJECTION *const injection = &m_Injections[m_NumInjections++];
    M_ReadVFile(injection, file, nullptr);
    M_InitialiseInjection(injection);
}

void Inject_AllInjections(void)
{
    if (m_Injections == nullptr) {
        return;
    }

    BENCHMARK benchmark = Benchmark_Start();

    for (int32_t i = 0; i < m_NumInjections; i++) {
        INJECTION *const injection = &m_Injections[i];
        if (!injection->relevant) {
            continue;
        }

        // Allow checks to be done on an injection's applicability after the
        // main level data has loaded.
        if (!M_IsApplicable(injection)) {
            LOG_WARNING(
                "Injection type %d is not applicable to the current level",
                injection->type);
            continue;
        }

        if (m_Context.mode != INJECTION_MODE_STATS) {
            LOG_DEBUG("Processing %s", injection->path);
        }

        // Cache the current status to allow individual handlers to increment
        // counts but still have access to current indices as required.
        m_CachedInfo = *Level_Context_GetInfo();

        const int32_t num_chunks = VFile_ReadS32(injection->fp);
        for (int32_t j = 0; j < num_chunks; j++) {
            const INJECTION_CHUNK chunk = M_ReadChunk(injection);
            if (chunk.type < 0 || chunk.type >= ICT_NUMBER_OF
                || m_Handlers[chunk.type] == nullptr) {
                LOG_WARNING("Unrecognised chunk type %d", chunk.type);
                VFile_Skip(injection->fp, chunk.total_size);
                continue;
            }

            m_Handlers[chunk.type](&m_Context, chunk);
        }

        ASSERT(VFile_GetPos(injection->fp) == injection->fp->size);
    }

    if (m_Context.mode != INJECTION_MODE_STATS) {
        Benchmark_End(&benchmark, nullptr);
    }
}

void Inject_Cleanup(void)
{
    if (m_Injections == nullptr) {
        return;
    }

    BENCHMARK benchmark = Benchmark_Start();

    for (int32_t i = 0; i < m_NumInjections; i++) {
        INJECTION *const injection = &m_Injections[i];
        if (injection->fp != nullptr) {
            VFile_Close(injection->fp);
        }
        Memory_FreePointer(&injection->path);
    }

    for (int32_t i = 0; i < IDT_NUMBER_OF; i++) {
        m_DataCounts[i] = 0;
    }
    m_MaxStaticObject3DId = -1;
    m_MaxStaticObject2DId = -1;

    Memory_FreePointer(&m_Injections);
    Memory_FreePointer(&m_PaletteMap);
    m_NumInjections = 0;
    m_CachedInfo = (LEVEL_CONTEXT_INFO) {};

    if (m_RoomMeta != nullptr) {
        Vector_Free(m_RoomMeta);
        m_RoomMeta = nullptr;
    }

    Benchmark_End(&benchmark, nullptr);
}

INJECTION_MESH_META Inject_GetRoomMeshMeta(const int32_t room_index)
{
    INJECTION_MESH_META summed_meta = {};
    if (m_RoomMeta == nullptr) {
        return summed_meta;
    }

    for (int32_t i = 0; i < m_RoomMeta->count; i++) {
        const INJECTION_MESH_META *const meta = Vector_Get(m_RoomMeta, i);
        if (meta->room_index != room_index) {
            continue;
        }

        summed_meta.num_vertices += meta->num_vertices;
        summed_meta.num_quads += meta->num_quads;
        summed_meta.num_triangles += meta->num_triangles;
        summed_meta.num_static_2ds += meta->num_static_2ds;
        summed_meta.num_static_3ds += meta->num_static_3ds;
    }

    return summed_meta;
}

int32_t Inject_GetDataCount(const INJECTION_DATA_TYPE type)
{
    return m_DataCounts[type];
}

int32_t Inject_GetMaxStaticObject3DId(void)
{
    return m_MaxStaticObject3DId;
}

int32_t Inject_GetMaxStaticObject2DId(void)
{
    return m_MaxStaticObject2DId;
}

LEVEL_CONTEXT_INFO Inject_GetCachedInfo(void)
{
    return m_CachedInfo;
}
