#include <trx/config.h>
#include <trx/config/types.h>
#include <trx/game/const.h>
#include <trx/game/gym.h>
#include <trx/game/lua/common.h>

#include <lauxlib.h>
#include <stdint.h>

static bool M_StoreAssaultTime(const float time)
{
    GYM_TRACK_STATS *const assault = &g_Config.profile.assault_stats;
    uint32_t logic_time = (uint32_t)(time * LOGIC_FPS);
    int32_t insert_idx = -1;

    for (int32_t i = 0; i < MAX_ASSAULT_TIMES; i++) {
        if (assault->entries[i].time == 0
            || logic_time < assault->entries[i].time) {
            insert_idx = i;
            break;
        }
    }
    if (insert_idx == -1) {
        return false;
    }

    for (int32_t i = MAX_ASSAULT_TIMES - 1; i > insert_idx; i--) {
        assault->entries[i] = assault->entries[i - 1];
    }

    assault->total_attempts++;
    assault->entries[insert_idx].time = logic_time;
    assault->entries[insert_idx].attempt_num = assault->total_attempts;
    Config_Update();
    return true;
}

static bool M_RemoveAssaultTimeAtIndex(const int32_t idx)
{
    GYM_TRACK_STATS *const assault = &g_Config.profile.assault_stats;
    if (idx < 0 || idx >= MAX_ASSAULT_TIMES) {
        return false;
    }
    if (assault->entries[idx].time == 0) {
        return false;
    }

    for (int32_t i = idx; i < MAX_ASSAULT_TIMES - 1; i++) {
        assault->entries[i] = assault->entries[i + 1];
    }

    assault->entries[MAX_ASSAULT_TIMES - 1].time = 0;
    assault->entries[MAX_ASSAULT_TIMES - 1].attempt_num = 0;
    Config_Update();
    return true;
}

// trxc.assault_stats.record(time) -> bool
static int M_L_AssaultStatsRecord(lua_State *const L)
{
    if (!Gym_TrackManager_HasStats(GYM_TRACK_ASSAULT)) {
        return luaL_error(L, "Assault stats unavailable");
    }
    const float time = (float)luaL_checknumber(L, 1);
    if (time <= 0.0f) {
        return luaL_error(L, "Time must be > 0");
    }

    const bool ok = M_StoreAssaultTime(time);
    lua_pushboolean(L, ok);
    return 1;
}

// trxc.assault_stats.remove(index) -> bool
static int M_L_AssaultStatsRemove(lua_State *const L)
{
    if (!Gym_TrackManager_HasStats(GYM_TRACK_ASSAULT)) {
        return luaL_error(L, "Assault stats unavailable");
    }
    const int64_t index_1 = luaL_checkinteger(L, 1);
    if (index_1 < 1 || index_1 > MAX_ASSAULT_TIMES) {
        return luaL_error(
            L, "Index out of range: %lld (expected 1..%d)", (long long)index_1,
            MAX_ASSAULT_TIMES);
    }

    const bool ok = M_RemoveAssaultTimeAtIndex(index_1 - 1);
    lua_pushboolean(L, ok);
    return 1;
}

// trxc.assault_stats.list() -> { { time = float, attempt_num = int }, ... }
static int M_L_AssaultStatsList(lua_State *const L)
{
    if (!Gym_TrackManager_HasStats(GYM_TRACK_ASSAULT)) {
        return luaL_error(L, "Assault stats unavailable");
    }

    const GYM_TRACK_STATS *const assault = &g_Config.profile.assault_stats;
    lua_newtable(L);
    int32_t out_idx = 1;

    for (int32_t i = 0; i < MAX_ASSAULT_TIMES; i++) {
        if (assault->entries[i].time == 0) {
            break;
        }

        lua_newtable(L);
        lua_pushnumber(
            L, (lua_Number)((float)assault->entries[i].time / LOGIC_FPS));
        lua_setfield(L, -2, "time");
        lua_pushinteger(L, (lua_Integer)assault->entries[i].attempt_num);
        lua_setfield(L, -2, "attempt_num");
        lua_seti(L, -2, out_idx);
        out_idx++;
    }

    return 1;
}

void LUA_CreateAssaultStats(lua_State *const L)
{
    lua_getglobal(L, "trxc");
    lua_newtable(L);

    lua_pushcfunction(L, M_L_AssaultStatsRecord);
    lua_setfield(L, -2, "record");
    lua_pushcfunction(L, M_L_AssaultStatsRemove);
    lua_setfield(L, -2, "remove");
    lua_pushcfunction(L, M_L_AssaultStatsList);
    lua_setfield(L, -2, "list");

    lua_setfield(L, -2, "assault_stats");
    lua_pop(L, 1);
}
