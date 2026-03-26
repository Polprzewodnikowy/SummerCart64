#include <trx/core/log.h>

#include <lauxlib.h>
#include <lua.h>

// trxc.log.log(level, msg)
static int M_L_LogGeneric(lua_State *const L)
{
    const LOG_LEVEL level = luaL_checkinteger(L, 1);
    const char *const msg = luaL_checkstring(L, 2);
    lua_Debug ar;
    const char *src = "?";
    const char *func = "?";
    int line = 0;
    if (lua_getstack(L, 2, &ar) && lua_getinfo(L, "nSl", &ar)) {
        src = ar.short_src;
        func = ar.name ? ar.name : "?";
        line = ar.currentline;
    }
    Log_Message(level, src, line, func, "%s", msg);
    return 0;
}

void LUA_CreateLog(lua_State *const L)
{
    lua_getglobal(L, "trxc");
    lua_newtable(L);

    lua_pushcfunction(L, M_L_LogGeneric);
    lua_setfield(L, -2, "log");

    lua_newtable(L);
    lua_pushinteger(L, LOG_LEVEL_INFO);
    lua_setfield(L, -2, "INFO");
    lua_pushinteger(L, LOG_LEVEL_WARNING);
    lua_setfield(L, -2, "WARNING");
    lua_pushinteger(L, LOG_LEVEL_ERROR);
    lua_setfield(L, -2, "ERROR");
    lua_pushinteger(L, LOG_LEVEL_DEBUG);
    lua_setfield(L, -2, "DEBUG");
    lua_setfield(L, -2, "LogLevel");

    lua_setfield(L, -2, "log");
    lua_pop(L, 1);
}
