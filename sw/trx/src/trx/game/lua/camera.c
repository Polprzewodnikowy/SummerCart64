#include <trx/game/camera/vars.h>
#include <trx/game/lua/common.h>
#include <trx/game/rooms/const.h>

#include <lauxlib.h>

// trxc.camera.get_pos() → {x, y, z}
static int M_L_CameraGetPos(lua_State *const L)
{
    lua_newtable(L);
    lua_pushinteger(L, g_Camera.pos.x);
    lua_setfield(L, -2, "x");
    lua_pushinteger(L, g_Camera.pos.y);
    lua_setfield(L, -2, "y");
    lua_pushinteger(L, g_Camera.pos.z);
    lua_setfield(L, -2, "z");
    return 1;
}

// trxc.camera.get_room() → int (1-based) or nil
static int M_L_CameraGetRoom(lua_State *const L)
{
    if (g_Camera.pos.room_num == NO_ROOM) {
        lua_pushnil(L);
    } else {
        lua_pushinteger(L, g_Camera.pos.room_num + 1);
    }
    return 1;
}

// trxc.camera.get_target_pos() → {x, y, z}
static int M_L_CameraGetTargetPos(lua_State *const L)
{
    lua_newtable(L);
    lua_pushinteger(L, g_Camera.target.x);
    lua_setfield(L, -2, "x");
    lua_pushinteger(L, g_Camera.target.y);
    lua_setfield(L, -2, "y");
    lua_pushinteger(L, g_Camera.target.z);
    lua_setfield(L, -2, "z");
    return 1;
}

// trxc.camera.get_target_room() → int (1-based) or nil
static int M_L_CameraGetTargetRoom(lua_State *const L)
{
    if (g_Camera.target.room_num == NO_ROOM) {
        lua_pushnil(L);
    } else {
        lua_pushinteger(L, g_Camera.target.room_num + 1);
    }
    return 1;
}

// trxc.camera.shake(intensity)
static int M_L_CameraShake(lua_State *const L)
{
    g_Camera.bounce = (int32_t)luaL_checkinteger(L, 1);
    return 0;
}

void LUA_CreateCamera(lua_State *const L)
{
    lua_getglobal(L, "trxc");
    lua_newtable(L);
    lua_pushcfunction(L, M_L_CameraGetPos);
    lua_setfield(L, -2, "get_pos");
    lua_pushcfunction(L, M_L_CameraGetRoom);
    lua_setfield(L, -2, "get_room");
    lua_pushcfunction(L, M_L_CameraGetTargetPos);
    lua_setfield(L, -2, "get_target_pos");
    lua_pushcfunction(L, M_L_CameraGetTargetRoom);
    lua_setfield(L, -2, "get_target_room");
    lua_pushcfunction(L, M_L_CameraShake);
    lua_setfield(L, -2, "shake");
    lua_setfield(L, -2, "camera");
    lua_pop(L, 1);
}
