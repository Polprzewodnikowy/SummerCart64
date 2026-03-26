#include <trx/game/objects.h>

#include <lauxlib.h>

// trxc.objects.swap_mesh(obj1_id, obj2_id, mesh1_num, mesh2_num)
static int M_L_ObjectsSwapMesh(lua_State *const L)
{
    const int32_t arg_count = lua_gettop(L);
    const OBJECT_ID obj1_id = luaL_checkinteger(L, 1);
    const OBJECT_ID obj2_id = luaL_checkinteger(L, 2);
    if (arg_count == 2) {
        Object_SwapAllMeshes(obj1_id, obj2_id);
    } else {
        const int32_t mesh1_num = luaL_checkinteger(L, 3);
        const int32_t mesh2_num = luaL_checkinteger(L, 4);
        Object_SwapMeshEx(obj1_id, obj2_id, mesh1_num, mesh2_num);
    }
    return 0;
}

void LUA_CreateObjects(lua_State *const L)
{
    lua_getglobal(L, "trxc");
    lua_newtable(L);
    lua_pushcfunction(L, M_L_ObjectsSwapMesh);
    lua_setfield(L, -2, "swap_mesh");
    lua_setfield(L, -2, "objects");
    lua_pop(L, 1);
}
