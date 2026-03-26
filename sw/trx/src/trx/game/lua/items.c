#include <trx/core/utils.h>
#include <trx/debug.h>
#include <trx/game/items.h>
#include <trx/game/lua/common.h>
#include <trx/game/objects.h>
#include <trx/game/rooms.h>

#include <lauxlib.h>

#define M_ITEM_GETTER(L)                                                       \
    const int idx = luaL_checkinteger(L, 1);                                   \
    const ITEM *const item = Item_Get(idx - 1);                                \
    if (item == nullptr) {                                                     \
        lua_pushnil(L);                                                        \
        return 1;                                                              \
    }

#define M_ITEM_SETTER(L)                                                       \
    const int idx = luaL_checkinteger(L, 1);                                   \
    ITEM *const item = Item_Get(idx - 1);                                      \
    if (item == nullptr) {                                                     \
        return 1;                                                              \
    }

// trxc.items.item_count() → int
static int M_L_ItemsCount(lua_State *const L)
{
    lua_pushinteger(L, Item_GetTotalCount());
    return 1;
}

// trxc.items.get(index or name) → int (1-based) or nil
static int M_L_ItemsGet(lua_State *const L)
{
    int result = 0;
    if (lua_type(L, 1) == LUA_TNUMBER) {
        const int idx = luaL_checkinteger(L, 1);
        const ITEM *const item = Item_Get(idx - 1);
        if (item != nullptr) {
            result = idx;
        }
    } else {
        const char *const name = luaL_checkstring(L, 1);
        const ITEM *const item = Item_GetByName(name);
        if (item != nullptr) {
            result = Item_GetIndex(item) + 1;
        }
    }
    if (result) {
        lua_pushinteger(L, result);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

// trxc.items.get_pos(index) → {x, y, z} or nil
static int M_L_ItemGetPos(lua_State *const L)
{
    M_ITEM_GETTER(L);
    lua_newtable(L);
    lua_pushinteger(L, item->pos.x);
    lua_setfield(L, -2, "x");
    lua_pushinteger(L, item->pos.y);
    lua_setfield(L, -2, "y");
    lua_pushinteger(L, item->pos.z);
    lua_setfield(L, -2, "z");
    return 1;
}

// trxc.items.get_rot(index) → {x, y, z} or nil
static int M_L_ItemGetRot(lua_State *const L)
{
    M_ITEM_GETTER(L);
    lua_newtable(L);
    lua_pushinteger(L, item->rot.x);
    lua_setfield(L, -2, "x");
    lua_pushinteger(L, item->rot.y);
    lua_setfield(L, -2, "y");
    lua_pushinteger(L, item->rot.z);
    lua_setfield(L, -2, "z");
    return 1;
}

// trxc.items.get_anim(index) → int or nil
static int M_L_ItemGetAnim(lua_State *const L)
{
    M_ITEM_GETTER(L);
    lua_pushinteger(L, Item_GetRelativeAnim(item));
    return 1;
}

// trxc.items.get_frame(index) → int or nil
static int M_L_ItemGetFrame(lua_State *const L)
{
    M_ITEM_GETTER(L);
    lua_pushinteger(L, Item_GetRelativeFrame(item));
    return 1;
}

// trxc.items.get_room(index) → int or nil
static int M_L_ItemGetRoom(lua_State *const L)
{
    M_ITEM_GETTER(L);
    lua_pushinteger(L, item->room_num + 1);
    return 1;
}

// trxc.items.get_status(index) → int or nil
static int M_L_ItemGetStatus(lua_State *const L)
{
    M_ITEM_GETTER(L);
    lua_pushinteger(L, (int)item->status);
    return 1;
}

// trxc.items.get_flags(index) → int or nil
static int M_L_ItemGetFlags(lua_State *const L)
{
    M_ITEM_GETTER(L);
    lua_pushinteger(L, (int)item->flags);
    return 1;
}

// trxc.items.get_timer(index) → int or nil
static int M_L_ItemGetTimer(lua_State *const L)
{
    M_ITEM_GETTER(L);
    lua_pushinteger(L, (int)item->timer);
    return 1;
}

// trxc.items.get_object_id(index) → int or nil
static int M_L_ItemGetObjectId(lua_State *const L)
{
    M_ITEM_GETTER(L);
    lua_pushinteger(L, item->object_id);
    return 1;
}

// trxc.items.get_hit_points(index) → int or nil
static int M_L_ItemGetHitPoints(lua_State *const L)
{
    M_ITEM_GETTER(L);
    lua_pushinteger(L, item->hit_points);
    return 1;
}

// trxc.items.get_max_hit_points(index) → int or nil
static int M_L_ItemGetMaxHitPoints(lua_State *const L)
{
    M_ITEM_GETTER(L);
    lua_pushinteger(L, item->max_hit_points);
    return 1;
}

// trxc.items.get_name(index) → string or nil
static int M_L_ItemGetName(lua_State *const L)
{
    M_ITEM_GETTER(L);
    if (item->name == nullptr) {
        lua_pushnil(L);
    } else {
        lua_pushstring(L, item->name);
    }
    return 1;
}

// trxc.items.set_pos(index, {x,y,z})
static int M_L_ItemSetPos(lua_State *const L)
{
    M_ITEM_SETTER(L);
    luaL_checktype(L, 2, LUA_TTABLE);
    lua_getfield(L, 2, "x");
    item->pos.x = luaL_checkinteger(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 2, "y");
    item->pos.y = luaL_checkinteger(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 2, "z");
    item->pos.z = luaL_checkinteger(L, -1);
    lua_pop(L, 1);
    item->room_num = Room_GetIndexFromPos(item->pos);
    return 0;
}

// trxc.items.set_anim(index, anim_idx)
static int M_L_ItemSetAnim(lua_State *const L)
{
    M_ITEM_SETTER(L);
    const int32_t anim_idx = luaL_checkinteger(L, 2);
    const OBJECT *const obj = Object_Get(item->object_id);
    if (obj->anim_idx == NO_ANIM) {
        return luaL_error(L, "object has no animations");
    }
    if (anim_idx < 0 || anim_idx >= Anim_GetTotalCount()
        || anim_idx >= obj->anim_count) {
        return luaL_error(L, "invalid animation index");
    }
    ANIM *const anim = Anim_GetAnim(obj->anim_idx + anim_idx);
    if (anim->frame_ptr == nullptr) {
        return luaL_error(L, "invalid animation index");
    }
    item->anim_num = obj->anim_idx + anim_idx;
    item->frame_num = anim->frame_base;
    return 0;
}

// trxc.items.set_frame(index, frame_idx)
static int M_L_ItemSetFrame(lua_State *const L)
{
    M_ITEM_SETTER(L);
    const int32_t frame_idx = luaL_checkinteger(L, 2);
    const OBJECT *const obj = Object_Get(item->object_id);
    if (obj->anim_idx == NO_ANIM) {
        return luaL_error(L, "object has no animations");
    }
    const ANIM *const anim = Item_GetAnim(item);
    if (frame_idx < 0) {
        if (anim->frame_end + frame_idx + 1 < anim->frame_base) {
            return luaL_error(L, "invalid frame index");
        }
        item->frame_num = anim->frame_end + frame_idx + 1;
    } else {
        if (anim->frame_base + frame_idx >= anim->frame_end) {
            return luaL_error(L, "invalid frame index");
        }
        item->frame_num = anim->frame_base + frame_idx;
    }
    return 0;
}

// trxc.items.set_hit_points(index, hp)
static int M_L_ItemSetHitPoints(lua_State *const L)
{
    M_ITEM_SETTER(L);
    item->hit_points = luaL_checkinteger(L, 2);
    item->max_hit_points = MAX(item->hit_points, item->max_hit_points);
    return 0;
}

// trxc.items.set_max_hit_points(index, max_hp)
static int M_L_ItemSetMaxHitPoints(lua_State *const L)
{
    M_ITEM_SETTER(L);
    item->max_hit_points = luaL_checkinteger(L, 2);
    return 0;
}

// trxc.items.set_rot(index, {x,y,z})
static int M_L_ItemSetRot(lua_State *const L)
{
    M_ITEM_SETTER(L);
    luaL_checktype(L, 2, LUA_TTABLE);
    lua_getfield(L, 2, "x");
    item->rot.x = luaL_checkinteger(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 2, "y");
    item->rot.y = luaL_checkinteger(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 2, "z");
    item->rot.z = luaL_checkinteger(L, -1);
    lua_pop(L, 1);
    return 0;
}

// trxc.items.set_name(index, name)
static int M_L_ItemSetName(lua_State *const L)
{
    M_ITEM_SETTER(L);
    const char *const new_name = luaL_checkstring(L, 2);
    if (!Item_SetName(Item_GetIndex(item), new_name)) {
        return luaL_error(L, "item name '%s' already in use", new_name);
    }
    return 0;
}

void LUA_CreateItems(lua_State *const L)
{
    lua_getglobal(L, "trxc");

    lua_newtable(L);
    lua_pushcfunction(L, M_L_ItemsCount);
    lua_setfield(L, -2, "count");
    lua_pushcfunction(L, M_L_ItemsGet);
    lua_setfield(L, -2, "get");
    lua_pushcfunction(L, M_L_ItemGetPos);
    lua_setfield(L, -2, "get_pos");
    lua_pushcfunction(L, M_L_ItemGetRot);
    lua_setfield(L, -2, "get_rot");
    lua_pushcfunction(L, M_L_ItemGetAnim);
    lua_setfield(L, -2, "get_anim");
    lua_pushcfunction(L, M_L_ItemGetFrame);
    lua_setfield(L, -2, "get_frame");
    lua_pushcfunction(L, M_L_ItemGetRoom);
    lua_setfield(L, -2, "get_room");
    lua_pushcfunction(L, M_L_ItemGetStatus);
    lua_setfield(L, -2, "get_status");
    lua_pushcfunction(L, M_L_ItemGetFlags);
    lua_setfield(L, -2, "get_flags");
    lua_pushcfunction(L, M_L_ItemGetTimer);
    lua_setfield(L, -2, "get_timer");
    lua_pushcfunction(L, M_L_ItemGetObjectId);
    lua_setfield(L, -2, "get_object_id");
    lua_pushcfunction(L, M_L_ItemGetHitPoints);
    lua_setfield(L, -2, "get_hit_points");
    lua_pushcfunction(L, M_L_ItemGetMaxHitPoints);
    lua_setfield(L, -2, "get_max_hit_points");
    lua_pushcfunction(L, M_L_ItemGetName);
    lua_setfield(L, -2, "get_name");
    lua_pushcfunction(L, M_L_ItemSetPos);
    lua_setfield(L, -2, "set_pos");
    lua_pushcfunction(L, M_L_ItemSetRot);
    lua_setfield(L, -2, "set_rot");
    lua_pushcfunction(L, M_L_ItemSetAnim);
    lua_setfield(L, -2, "set_anim");
    lua_pushcfunction(L, M_L_ItemSetFrame);
    lua_setfield(L, -2, "set_frame");
    lua_pushcfunction(L, M_L_ItemSetHitPoints);
    lua_setfield(L, -2, "set_hit_points");
    lua_pushcfunction(L, M_L_ItemSetMaxHitPoints);
    lua_setfield(L, -2, "set_max_hit_points");
    lua_pushcfunction(L, M_L_ItemSetName);
    lua_setfield(L, -2, "set_name");
    lua_setfield(L, -2, "items");
    lua_pop(L, 1);
}
