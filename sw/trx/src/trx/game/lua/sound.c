#include <trx/game/lua/common.h>
#include <trx/game/sound/common.h>

#include <lauxlib.h>

// trxc.sound.is_available(id)
static int M_L_SoundIsAvailable(lua_State *const L)
{
    const SAMPLE_ID id = (SAMPLE_ID)luaL_checkinteger(L, 1);
    lua_pushboolean(L, Sound_IsAvailable_Direct(id));
    return 1;
}

// trxc.sound.play(id[, opts])
static int M_L_SoundPlay(lua_State *const L)
{
    const SAMPLE_ID id = (SAMPLE_ID)luaL_checkinteger(L, 1);
    if (!Sound_IsAvailable_Direct(id)) {
        return luaL_error(L, "invalid sound track: %d", (int)id);
    }
    XYZ_32 pos;
    const XYZ_32 *pos_ptr = nullptr;
    if (lua_gettop(L) >= 2 && lua_istable(L, 2)) {
        lua_getfield(L, 2, "pos");
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "x");
            pos.x = (int32_t)luaL_optinteger(L, -1, 0);
            lua_pop(L, 1);
            lua_getfield(L, -1, "y");
            pos.y = (int32_t)luaL_optinteger(L, -1, 0);
            lua_pop(L, 1);
            lua_getfield(L, -1, "z");
            pos.z = (int32_t)luaL_optinteger(L, -1, 0);
            lua_pop(L, 1);
            pos_ptr = &pos;
        }
        lua_pop(L, 1);
    }
    Sound_Effect_Direct(id, pos_ptr, SPM_ALWAYS | SPM_STATIC_POS);
    return 0;
}

// trxc.sound.stop(id)
static int M_L_SoundStop(lua_State *const L)
{
    const SAMPLE_ID id = (SAMPLE_ID)luaL_checkinteger(L, 1);
    Sound_StopEffect_Direct(id);
    return 0;
}

// trxc.sound.stop_all()
static int M_L_SoundStopAll(lua_State *const L)
{
    Sound_StopAll();
    return 0;
}

void LUA_CreateSound(lua_State *const L)
{
    lua_getglobal(L, "trxc");
    lua_newtable(L);
    lua_pushcfunction(L, M_L_SoundIsAvailable);
    lua_setfield(L, -2, "is_available");
    lua_pushcfunction(L, M_L_SoundPlay);
    lua_setfield(L, -2, "play");
    lua_pushcfunction(L, M_L_SoundStop);
    lua_setfield(L, -2, "stop");
    lua_pushcfunction(L, M_L_SoundStopAll);
    lua_setfield(L, -2, "stop_all");
    lua_setfield(L, -2, "sound");
    lua_pop(L, 1);
}
