#include <trx/config/common.h>
#include <trx/game/lua/common.h>

#include <lauxlib.h>

// trxc.config.get(key)
static int M_L_ConfigGet(lua_State *const L)
{
    const char *const key = luaL_checkstring(L, 1);
    const CONFIG_OPTION *const opt = Config_GetOptionByPath(key);
    if (opt == nullptr) {
        return luaL_error(L, "Unknown option: %s", key);
    }
    const char *const value = Config_GetOptionValueAsString(opt, false);
    lua_pushstring(L, value);
    return 1;
}

// trxc.config.set(key, value)
static int M_L_ConfigSet(lua_State *const L)
{
    const char *const key = luaL_checkstring(L, 1);
    const char *const new_value = luaL_checkstring(L, 2);
    const CONFIG_OPTION *const opt = Config_GetOptionByPath(key);
    if (opt == nullptr) {
        return luaL_error(L, "Unknown option: %s", key);
    }
    const bool ok = Config_SetOptionValueFromString(opt, new_value);
    if (!ok) {
        return luaL_error(L, "Failed to set option %s to %s", key, new_value);
    }
    Config_Update();
    return 0;
}

// trxc.config.list()
static int M_L_ConfigList(lua_State *const L)
{
    lua_newtable(L);
    const CONFIG_OPTION *opt = Config_GetOptionMap();
    while (opt->name != nullptr) {
        const char *const value = Config_GetOptionValueAsString(opt, false);
        lua_pushstring(L, value);
        lua_setfield(L, -2, opt->name);
        opt++;
    }
    return 1;
}

void LUA_CreateConfig(lua_State *const L)
{
    lua_getglobal(L, "trxc");
    lua_newtable(L);
    lua_pushcfunction(L, M_L_ConfigGet);
    lua_setfield(L, -2, "get");
    lua_pushcfunction(L, M_L_ConfigSet);
    lua_setfield(L, -2, "set");
    lua_pushcfunction(L, M_L_ConfigList);
    lua_setfield(L, -2, "list");
    lua_setfield(L, -2, "config");
    lua_pop(L, 1);
}
