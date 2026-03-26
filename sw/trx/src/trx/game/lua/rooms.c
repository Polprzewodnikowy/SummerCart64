#include <trx/game/items/actions/ids.h>
#include <trx/game/lua/common.h>
#include <trx/game/rooms.h>

#include <lauxlib.h>

#define M_ROOM_GETTER(L)                                                       \
    const int idx = luaL_checkinteger(L, 1);                                   \
    const ROOM *const room = Room_Get(idx - 1);                                \
    if (room == nullptr) {                                                     \
        lua_pushnil(L);                                                        \
        return 1;                                                              \
    }

#define M_ROOM_SETTER(L)                                                       \
    const int idx = luaL_checkinteger(L, 1);                                   \
    ROOM *const room = Room_Get(idx - 1);                                      \
    if (room == nullptr) {                                                     \
        return 1;                                                              \
    }

// trxc.rooms.count() → int
static int M_L_RoomsCount(lua_State *const L)
{
    lua_pushinteger(L, Room_GetCount());
    return 1;
}

// trxc.rooms.get(index) → int (1-based) or nil
static int M_L_RoomGet(lua_State *const L)
{
    const int idx = luaL_checkinteger(L, 1);
    const ROOM *const room = Room_Get(idx - 1);
    if (room == nullptr) {
        lua_pushnil(L);
    } else {
        lua_pushinteger(L, idx);
    }
    return 1;
}

// trxc.rooms.get_underwater(index) → bool or nil
static int M_L_RoomGetUnderwater(lua_State *const L)
{
    M_ROOM_GETTER(L);
    lua_pushboolean(L, room->flags.underwater);
    return 1;
}

// trxc.rooms.get_wind(index) → bool or nil
static int M_L_RoomGetWind(lua_State *const L)
{
    M_ROOM_GETTER(L);
    lua_pushboolean(L, room->flags.wind);
    return 1;
}

// trxc.rooms.get_flip_status(index) → integer
static int M_L_RoomGetFlipStatus(lua_State *const L)
{
    M_ROOM_GETTER(L);
    lua_pushinteger(L, room->flip_status);
    return 1;
}

// trxc.rooms.get_flip_room(index) → integer or nil
static int M_L_RoomGetFlippedRoom(lua_State *const L)
{
    M_ROOM_GETTER(L);
    if (room->flipped_room == NO_ROOM) {
        lua_pushnil(L);
    } else {
        lua_pushinteger(L, room->flipped_room + 1);
    }
    return 1;
}

// trxc.rooms.set_underwater(index, bool)
static int M_L_RoomSetUnderwater(lua_State *const L)
{
    M_ROOM_SETTER(L);
    room->flags.underwater = lua_toboolean(L, 2);
    return 1;
}

// trxc.rooms.set_wind(index, bool)
static int M_L_RoomSetWind(lua_State *const L)
{
    M_ROOM_SETTER(L);
    room->flags.wind = lua_toboolean(L, 2);
    return 1;
}

// trxc.rooms.get_bounds() → table
static int M_L_RoomGetBounds(lua_State *const L)
{
    M_ROOM_GETTER(L);
    const BOUNDS_32 bounds = Room_GetRoomBounds(Room_Get(idx - 1));
    lua_newtable(L);
    lua_pushinteger(L, bounds.min.x);
    lua_setfield(L, -2, "min_x");
    lua_pushinteger(L, bounds.min.y);
    lua_setfield(L, -2, "min_y");
    lua_pushinteger(L, bounds.min.z);
    lua_setfield(L, -2, "min_z");
    lua_pushinteger(L, bounds.max.x);
    lua_setfield(L, -2, "max_x");
    lua_pushinteger(L, bounds.max.y);
    lua_setfield(L, -2, "max_y");
    lua_pushinteger(L, bounds.max.z);
    lua_setfield(L, -2, "max_z");
    return 1;
}

// trxc.rooms.flip()
static int M_L_RoomFlip(lua_State *const L)
{
    Room_FlipMap();
    return 0;
}

// trxc.rooms.flip_effect(effect_id, [timer])
static int M_L_RoomFlipEffect(lua_State *const L)
{
    const int32_t trx_effect_id = luaL_checkinteger(L, 1);
    if (trx_effect_id == -1) {
        Room_SetFlipEffect(-1);
    } else {
        const ITEM_ACTION game_id = ItemAction_ToGameID(trx_effect_id);
        if (game_id == ITEM_ACTION_INVALID) {
            return luaL_error(L, "invalid flip effect id");
        }
        Room_SetFlipEffect(game_id);
    }

    const int arg_count = lua_gettop(L);
    if (arg_count >= 2) {
        const int32_t timer = luaL_checkinteger(L, 2);
        Room_SetFlipTimer(timer);
    }

    return 0;
}

void LUA_CreateRooms(lua_State *const L)
{
    lua_getglobal(L, "trxc");
    lua_newtable(L);

    lua_newtable(L);
    lua_pushinteger(L, RFS_UNFLIPPED);
    lua_setfield(L, -2, "UNFLIPPED");
    lua_pushinteger(L, RFS_FLIPPED);
    lua_setfield(L, -2, "FLIPPED");
    lua_pushinteger(L, RFS_NONE);
    lua_setfield(L, -2, "NONE");
    lua_setfield(L, -2, "FlipStatus");

    lua_pushcfunction(L, M_L_RoomsCount);
    lua_setfield(L, -2, "count");
    lua_pushcfunction(L, M_L_RoomGet);
    lua_setfield(L, -2, "get");
    lua_pushcfunction(L, M_L_RoomGetUnderwater);
    lua_setfield(L, -2, "get_underwater");
    lua_pushcfunction(L, M_L_RoomGetWind);
    lua_setfield(L, -2, "get_wind");
    lua_pushcfunction(L, M_L_RoomSetUnderwater);
    lua_setfield(L, -2, "set_underwater");
    lua_pushcfunction(L, M_L_RoomSetWind);
    lua_setfield(L, -2, "set_wind");
    lua_pushcfunction(L, M_L_RoomGetBounds);
    lua_setfield(L, -2, "get_bounds");
    lua_pushcfunction(L, M_L_RoomGetFlipStatus);
    lua_setfield(L, -2, "get_flip_status");
    lua_pushcfunction(L, M_L_RoomGetFlippedRoom);
    lua_setfield(L, -2, "get_flipped_room");
    lua_pushcfunction(L, M_L_RoomFlip);
    lua_setfield(L, -2, "flip");
    lua_pushcfunction(L, M_L_RoomFlipEffect);
    lua_setfield(L, -2, "flip_effect");
    lua_setfield(L, -2, "rooms");
    lua_pop(L, 1);
}
