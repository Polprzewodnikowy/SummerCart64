#include <trx/game/stats/init.h>

#include <trx/config.h>
#include <trx/core/benchmark.h>
#include <trx/core/hash.h>
#include <trx/core/json.h>
#include <trx/core/log.h>
#include <trx/core/memory.h>
#include <trx/core/virtual_file.h>
#include <trx/debug.h>
#include <trx/game/creature.h>
#include <trx/game/game_buf.h>
#include <trx/game/game_flow.h>
#include <trx/game/inject.h>
#include <trx/game/items/carrier.h>
#include <trx/game/level.h>
#include <trx/game/level/cache.h>
#include <trx/game/level/format/format.h>
#include <trx/game/lua.h>
#include <trx/game/rooms.h>
#include <trx/game/stats.h>

#include <string.h>

#define M_CACHE_VERSION 4
#define M_CACHE_FILENAME "max_stats.cache.json"

static LEVEL_MAX_STATS *m_Stats = nullptr;
static int32_t m_StatsCapacity = 0;

static void M_EnsureStatsStorage(const int32_t level_count)
{
    ASSERT(level_count >= 0);

    if (m_StatsCapacity != level_count) {
        m_Stats = Memory_Realloc(
            m_Stats, sizeof(LEVEL_MAX_STATS) * (size_t)level_count);
        m_StatsCapacity = level_count;
    }
}

static uint64_t M_ComputeInputsChecksum(const GF_LEVEL_TABLE *const level_table)
{
    uint64_t hash = LevelCache_InitChecksum("max_stats_cache", M_CACHE_VERSION);
    hash = Hash_FNV1a64_UpdateU32(hash, (uint32_t)level_table->count);
    hash = Hash_FNV1a64_UpdateU32(
        hash, (uint32_t)g_Config.gameplay.restore_ps1_enemies);

    for (int32_t i = 0; i < level_table->count; i++) {
        const GF_LEVEL *const level = GF_GetLevel(GFLT_MAIN, i);
        hash = LevelCache_UpdateLevelChecksum(hash, level);
    }

    return hash;
}

static JSON_OBJECT *M_SerializeLevelMaxStats(const LEVEL_MAX_STATS *const stats)
{
    JSON_OBJECT *const out = JSON_ObjectNew();

    JSON_ObjectAppendInt64(
        out, "max_pickup_secret_count",
        (int64_t)stats->max_pickup_secret_count);
    JSON_ObjectAppendInt64(
        out, "max_kill_count", (int64_t)stats->max_kill_count);
    JSON_ObjectAppendInt64(
        out, "max_kill_ally_count", (int64_t)stats->max_kill_ally_count);
    JSON_ObjectAppendInt64(
        out, "max_kill_non_ally_count",
        (int64_t)stats->max_kill_non_ally_count);
    JSON_ObjectAppendInt64(
        out, "max_pickup_count", (int64_t)stats->max_pickup_count);
    JSON_ObjectAppendInt64(
        out, "max_secret_count", (int64_t)stats->max_secret_count);
    JSON_ObjectAppendInt64(out, "all_secrets_mask", stats->all_secrets_mask);

    JSON_ARRAY *const secret_item_masks = JSON_ArrayNew();
    for (int32_t i = 0; i < STATS_MAX_SECRETS; i++) {
        JSON_OBJECT *const entry = JSON_ObjectNew();
        JSON_ObjectAppendInt(
            entry, "item_num", stats->secret_item_masks[i].item_num);
        JSON_ObjectAppendInt64(
            entry, "secret_mask",
            (int64_t)stats->secret_item_masks[i].secret_mask);
        JSON_ArrayAppendObject(secret_item_masks, entry);
    }
    JSON_ObjectAppendArray(out, "secret_item_masks", secret_item_masks);

    JSON_ARRAY *const secret_objects = JSON_ArrayNew();
    for (int32_t i = 0; i < STATS_MAX_SECRETS; i++) {
        JSON_OBJECT *const entry = JSON_ObjectNew();
        JSON_ObjectAppendBool(entry, "taken", stats->secret_objects[i].taken);
        JSON_ObjectAppendInt(
            entry, "assigned_object_id",
            (int32_t)stats->secret_objects[i].assigned_object_id);
        JSON_ObjectAppendInt(
            entry, "item_num", stats->secret_objects[i].item_num);
        JSON_ArrayAppendObject(secret_objects, entry);
    }
    JSON_ObjectAppendArray(out, "secret_objects", secret_objects);

    return out;
}

static bool M_DeserializeLevelMaxStats(
    LEVEL_MAX_STATS *const out, JSON_OBJECT *const obj)
{
    if (out == nullptr || obj == nullptr) {
        return false;
    }

    out->max_pickup_secret_count = (size_t)JSON_ObjectGetInt64(
        obj, "max_pickup_secret_count", (int64_t)out->max_pickup_secret_count);
    out->max_kill_count = (size_t)JSON_ObjectGetInt64(
        obj, "max_kill_count", (int64_t)out->max_kill_count);
    out->max_kill_ally_count = (size_t)JSON_ObjectGetInt64(
        obj, "max_kill_ally_count", (int64_t)out->max_kill_ally_count);
    out->max_kill_non_ally_count = (size_t)JSON_ObjectGetInt64(
        obj, "max_kill_non_ally_count", (int64_t)out->max_kill_non_ally_count);
    out->max_pickup_count = (size_t)JSON_ObjectGetInt64(
        obj, "max_pickup_count", (int64_t)out->max_pickup_count);
    out->max_secret_count = (size_t)JSON_ObjectGetInt64(
        obj, "max_secret_count", (int64_t)out->max_secret_count);
    out->all_secrets_mask = (uint32_t)JSON_ObjectGetInt64(
        obj, "all_secrets_mask", out->all_secrets_mask);

    JSON_ARRAY *const secret_item_masks =
        JSON_ObjectGetArray(obj, "secret_item_masks");
    if (secret_item_masks != nullptr
        && secret_item_masks->length == (size_t)STATS_MAX_SECRETS) {
        for (int32_t i = 0; i < STATS_MAX_SECRETS; i++) {
            JSON_OBJECT *const entry =
                JSON_ArrayGetObject(secret_item_masks, i);
            if (entry == nullptr) {
                continue;
            }
            out->secret_item_masks[i].item_num = JSON_ObjectGetInt(
                entry, "item_num", out->secret_item_masks[i].item_num);
            out->secret_item_masks[i].secret_mask =
                (uint32_t)JSON_ObjectGetInt64(
                    entry, "secret_mask",
                    out->secret_item_masks[i].secret_mask);
        }
    }

    JSON_ARRAY *const secret_objects =
        JSON_ObjectGetArray(obj, "secret_objects");
    if (secret_objects != nullptr
        && secret_objects->length == (size_t)STATS_MAX_SECRETS) {
        for (int32_t i = 0; i < STATS_MAX_SECRETS; i++) {
            JSON_OBJECT *const entry = JSON_ArrayGetObject(secret_objects, i);
            if (entry == nullptr) {
                continue;
            }
            out->secret_objects[i].taken = JSON_ObjectGetBool(
                entry, "taken", out->secret_objects[i].taken);
            out->secret_objects[i].assigned_object_id =
                (OBJECT_ID)JSON_ObjectGetInt(
                    entry, "assigned_object_id",
                    (int32_t)out->secret_objects[i].assigned_object_id);
            out->secret_objects[i].item_num = JSON_ObjectGetInt(
                entry, "item_num", out->secret_objects[i].item_num);
        }
    }

    return true;
}

static bool M_TryLoadCache(
    const uint64_t expected_checksum, const GF_LEVEL_TABLE *const level_table)
{
    JSON_VALUE *const root_value =
        LevelCache_ReadJSON(M_CACHE_FILENAME, expected_checksum);
    if (root_value == nullptr) {
        return false;
    }

    JSON_OBJECT *const root = JSON_ValueAsObject(root_value);
    if (root == nullptr) {
        JSON_ValueFree(root_value);
        return false;
    }

    const int32_t version = JSON_ObjectGetInt(root, "version", -1);
    if (version != M_CACHE_VERSION) {
        JSON_ValueFree(root_value);
        return false;
    }

    const int32_t cached_level_count =
        JSON_ObjectGetInt(root, "level_count", -1);
    if (cached_level_count != level_table->count) {
        JSON_ValueFree(root_value);
        return false;
    }

    JSON_ARRAY *const levels = JSON_ObjectGetArray(root, "levels");
    if (levels == nullptr) {
        JSON_ValueFree(root_value);
        return false;
    }

    // Clear any existing stats; cache may not cover every entry.
    memset(m_Stats, 0, sizeof(LEVEL_MAX_STATS) * (size_t)m_StatsCapacity);

    for (size_t i = 0; i < levels->length; i++) {
        JSON_OBJECT *const entry = JSON_ArrayGetObject(levels, i);
        if (entry == nullptr) {
            continue;
        }
        const int32_t level_num = JSON_ObjectGetInt(entry, "num", -1);
        if (level_num < 0 || level_num >= m_StatsCapacity) {
            continue;
        }
        JSON_OBJECT *const stats_obj = JSON_ObjectGetObject(entry, "stats");
        if (stats_obj == nullptr) {
            continue;
        }
        M_DeserializeLevelMaxStats(&m_Stats[level_num], stats_obj);
    }

    JSON_ValueFree(root_value);
    return true;
}

static void M_WriteCache(
    const uint64_t checksum, const GF_LEVEL_TABLE *const level_table)
{
    JSON_OBJECT *const root = JSON_ObjectNew();
    JSON_ObjectAppendInt(root, "version", M_CACHE_VERSION);
    JSON_ObjectAppendInt(root, "level_count", level_table->count);

    JSON_ARRAY *const levels = JSON_ArrayNew();
    for (int32_t i = 0; i < level_table->count; i++) {
        const GF_LEVEL *const level = GF_GetLevel(GFLT_MAIN, i);
        JSON_OBJECT *const entry = JSON_ObjectNew();
        JSON_ObjectAppendInt(entry, "num", level->num);
        JSON_ObjectAppendObject(
            entry, "stats", M_SerializeLevelMaxStats(&m_Stats[level->num]));
        JSON_ArrayAppendObject(levels, entry);
    }
    JSON_ObjectAppendArray(root, "levels", levels);

    JSON_VALUE *const root_value = JSON_ValueFromObject(root);
    LevelCache_WriteJSON(M_CACHE_FILENAME, checksum, root_value);
    JSON_ValueFree(root_value);
}

__attribute__((destructor)) static void M_Shutdown(void)
{
    if (m_Stats != nullptr) {
        Memory_Free(m_Stats);
        m_Stats = nullptr;
    }
    m_StatsCapacity = 0;
}

LEVEL_MAX_STATS *Stats_GetLevelMaxStats(const GF_LEVEL *const level)
{
    ASSERT(m_Stats != nullptr);
    ASSERT(level != nullptr);
    ASSERT(level->num >= 0 && level->num < m_StatsCapacity);
    return &m_Stats[level->num];
}

void Stats_CalculateMaxStats(void)
{
    const GF_LEVEL_TABLE *const level_table = GF_GetLevelTable(GFLT_MAIN);
    M_EnsureStatsStorage(level_table->count);
    memset(m_Stats, 0, sizeof(LEVEL_MAX_STATS) * (size_t)m_StatsCapacity);

    BENCHMARK benchmark = Benchmark_Start();
    const uint64_t expected_checksum = M_ComputeInputsChecksum(level_table);
    if (M_TryLoadCache(expected_checksum, level_table)) {
        goto finish;
    }

    for (int32_t i = 0; i < level_table->count; i++) {
        const GF_LEVEL *const level = GF_GetLevel(GFLT_MAIN, i);
        if (level->type != GFL_NORMAL && level->type != GFL_BONUS) {
            continue;
        }

        VFILE *const file = VFile_CreateFromPath(level->path);
        if (file == nullptr) {
            continue;
        }

        const LEVEL_FORMAT_LOADER *const loader =
            Level_Format_GuessLoader(file);
        if (loader != nullptr) {
            Level_Unload();
            Creature_Reset();

            Lua_ClearLevelListeners();
            Lua_SetScriptContext(LUA_CONTEXT_LEVEL);
            if (level->script_path != nullptr) {
                LUA_RESULT res = Lua_EvalFile(level->script_path);
                if (res.code != LUA_OK) {
                    LOG_ERROR("Lua level script error: %s", res.message);
                }
                Lua_FreeResult(&res);
            }
            Lua_SetScriptContext(LUA_CONTEXT_GLOBAL);
            Lua_FireEventInt32(LUA_EVENT_BEFORE_LEVEL_FILE, level->num);

            Inject_InitLevel(level, INJECTION_MODE_STATS);
            if (loader->probe(loader, file, LEVEL_FORMAT_PROBE_STATS)) {
                Inject_AllInjections();

                for (int32_t item_num = 0; item_num < Item_GetTotalCount();
                     item_num++) {
                    ITEM *const item = Item_Get(item_num);
                    ROOM *const room = Room_Get(item->room_num);
                    item->next_item = room->item_num;
                    room->item_num = item_num;
                }
                Carrier_InitialiseLevel(level);

                Stats_ScanLevel(level);
            }
            Inject_Cleanup();
        }

        GameBuf_Reset();
        VFile_Close(file);

#if 0
        const LEVEL_MAX_STATS *const max_stats = Stats_GetLevelMaxStats(level);
        LOG_INFO(
            "Level %d (%s)", GF_GetLevelOrdinalNumber(GFLT_MAIN, level),
            level->title);
        LOG_INFO("    pickups:   %d", max_stats->max_pickup_count);
        LOG_INFO("    kills:     %d", max_stats->max_kill_count);
        LOG_INFO("      allies:  %d", max_stats->max_kill_ally_count);
        LOG_INFO("      enemies: %d", max_stats->max_kill_non_ally_count);
        LOG_INFO("    secrets:   %d", max_stats->max_secret_count);
#endif
    }

    M_WriteCache(expected_checksum, level_table);

finish:
    const FINAL_STATS final_stats = Stats_ComputeFinalStats(true);
    LOG_INFO("Max pickups: %d", final_stats.max_stats.max_pickup_count);
    LOG_INFO("Max kills:   %d", final_stats.max_stats.max_kill_count);
    LOG_INFO("  allies:    %d", final_stats.max_stats.max_kill_ally_count);
    LOG_INFO("  enemies:   %d", final_stats.max_stats.max_kill_non_ally_count);
    LOG_INFO("Max secrets: %d", final_stats.max_stats.max_secret_count);
    Benchmark_End(&benchmark, nullptr);
}
