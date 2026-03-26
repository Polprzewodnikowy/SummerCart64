#include <trx/game/lua/events.h>

#include <trx/core/log.h>
#include <trx/core/vector.h>
#include <trx/game/lua/common.h>

#include <lauxlib.h>
#include <lua.h>

typedef struct {
    int32_t ref;
    LUA_EVENT_TYPE type;
    bool level_scoped;
} M_LISTENER;

static lua_State *m_L = nullptr;
static VECTOR *m_Listeners = nullptr;

static void M_ClearAllListeners(const bool unref_from_lua)
{
    if (m_Listeners == nullptr) {
        return;
    }

    if (unref_from_lua && m_L != nullptr) {
        for (int32_t i = 0; i < m_Listeners->count; i++) {
            const M_LISTENER *const lst = Vector_Get(m_Listeners, i);
            luaL_unref(m_L, LUA_REGISTRYINDEX, lst->ref);
        }
    }

    Vector_Free(m_Listeners);
    m_Listeners = nullptr;
}

__attribute__((destructor)) static void M_Shutdown(void)
{
    M_ClearAllListeners(false);
    m_L = nullptr;
}

void Lua_ShutdownEvents(void)
{
    M_ClearAllListeners(true);
    m_L = nullptr;
}

// trxc.events.attach(event_type, callback) → id
static int32_t M_L_EventsAttach(lua_State *const L)
{
    const LUA_EVENT_TYPE ev = luaL_checkinteger(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushvalue(L, 2);
    const int32_t ref = luaL_ref(L, LUA_REGISTRYINDEX);
    if (m_Listeners == nullptr) {
        m_Listeners = Vector_Create(sizeof(M_LISTENER));
    }
    const M_LISTENER listener = {
        .ref = ref,
        .type = ev,
        .level_scoped = Lua_GetScriptContext() == LUA_CONTEXT_LEVEL,
    };
    Vector_Add(m_Listeners, &listener);
    lua_pushinteger(L, ref);
    return 1;
}

// trxc.events.detach(id)
static int32_t M_L_EventsDetach(lua_State *const L)
{
    int32_t id = luaL_checkinteger(L, 1);
    if (m_Listeners == nullptr) {
        return 0;
    }
    for (int32_t i = 0; i < m_Listeners->count; i++) {
        const M_LISTENER *const lst = Vector_Get(m_Listeners, i);
        if (lst->ref == id) {
            luaL_unref(L, LUA_REGISTRYINDEX, lst->ref);
            Vector_RemoveAt(m_Listeners, i);
            break;
        }
    }
    return 0;
}

void Lua_ClearLevelListeners(void)
{
    lua_State *const L = m_L;
    if (L == nullptr) {
        return;
    }
    if (m_Listeners == nullptr) {
        return;
    }
    for (int32_t i = 0; i < m_Listeners->count;) {
        M_LISTENER *const lst = Vector_Get(m_Listeners, i);
        if (lst->level_scoped) {
            luaL_unref(L, LUA_REGISTRYINDEX, lst->ref);
            Vector_RemoveAt(m_Listeners, i);
        } else {
            i++;
        }
    }
}

static void M_PushArg(lua_State *const L, const LUA_EVENT_ARG arg)
{
    switch (arg.type) {
    case LUA_EVENT_ARG_NIL:
        lua_pushnil(L);
        break;
    case LUA_EVENT_ARG_INT32:
        lua_pushinteger(L, arg.value.i32);
        break;
    case LUA_EVENT_ARG_BOOL:
        lua_pushboolean(L, arg.value.b);
        break;
    case LUA_EVENT_ARG_NUMBER:
        lua_pushnumber(L, arg.value.number);
        break;
    case LUA_EVENT_ARG_STRING:
        if (arg.value.str != nullptr) {
            lua_pushstring(L, arg.value.str);
        } else {
            lua_pushnil(L);
        }
        break;
    }
}

void Lua_FireEventEx(
    const LUA_EVENT_TYPE ev, const LUA_EVENT_ARG *const args,
    const int32_t arg_count)
{
    lua_State *const L = m_L;
    if (L == nullptr || m_Listeners == nullptr) {
        return;
    }
    for (int32_t i = 0; i < m_Listeners->count; i++) {
        M_LISTENER *const lst = Vector_Get(m_Listeners, i);
        if (lst->type != ev) {
            continue;
        }
        lua_rawgeti(L, LUA_REGISTRYINDEX, lst->ref);
        for (int32_t arg_idx = 0; arg_idx < arg_count; arg_idx++) {
            M_PushArg(L, args[arg_idx]);
        }
        if (lua_pcall(L, arg_count, 0, 0) != LUA_OK) {
            LOG_ERROR("Lua event handler error: %s", lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }
}

void Lua_FireEventInt32(const LUA_EVENT_TYPE ev, const int32_t arg)
{
    const LUA_EVENT_ARG args[] = {
        { .type = LUA_EVENT_ARG_INT32, .value = { .i32 = arg } },
    };
    Lua_FireEventEx(ev, args, 1);
}

void LUA_CreateEvents(lua_State *const L)
{
    m_L = L;
    lua_getglobal(L, "trxc");
    lua_newtable(L);

    lua_pushcfunction(L, M_L_EventsAttach);
    lua_setfield(L, -2, "attach");
    lua_pushcfunction(L, M_L_EventsDetach);
    lua_setfield(L, -2, "detach");

    lua_newtable(L);
    lua_pushinteger(L, LUA_EVENT_BEFORE_LEVEL_FILE);
    lua_setfield(L, -2, "BEFORE_LEVEL_FILE");
    lua_pushinteger(L, LUA_EVENT_AFTER_LEVEL_FILE);
    lua_setfield(L, -2, "AFTER_LEVEL_FILE");
    lua_pushinteger(L, LUA_EVENT_AFTER_LEVEL_STATE);
    lua_setfield(L, -2, "AFTER_LEVEL_STATE");
    lua_pushinteger(L, LUA_EVENT_GAME_START);
    lua_setfield(L, -2, "GAME_START");
    lua_pushinteger(L, LUA_EVENT_PICKUP);
    lua_setfield(L, -2, "PICKUP");
    lua_pushinteger(L, LUA_EVENT_BEFORE_CONTROL);
    lua_setfield(L, -2, "BEFORE_CONTROL");
    lua_pushinteger(L, LUA_EVENT_AFTER_CONTROL);
    lua_setfield(L, -2, "AFTER_CONTROL");
    lua_setfield(L, -2, "EventType");

    lua_setfield(L, -2, "events");
    lua_pop(L, 1);
}
