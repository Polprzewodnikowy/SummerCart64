#include <trx/game/lua/common.h>

#include <trx/core/filesystem.h>
#include <trx/core/log.h>
#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/debug.h>
#include <trx/game/game_flow/common.h>
#include <trx/game/lua/embedded_scripts.h>
#include <trx/game/lua/events.h>

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <string.h>

typedef struct {
    lua_State *state;
    LUA_CONTEXT context;
} M_PRIV;

static M_PRIV m_Priv = {
    .context = LUA_CONTEXT_GLOBAL,
};

// Initialize internal APIs
extern void LUA_CreateCatalog(lua_State *L);
extern void LUA_CreateCamera(lua_State *L);
extern void LUA_CreateConsole(lua_State *L);
extern void LUA_CreateEvents(lua_State *L);
extern void LUA_CreateItems(lua_State *L);
extern void LUA_CreateLara(lua_State *L);
extern void LUA_CreateLog(lua_State *L);
extern void LUA_CreateMusic(lua_State *L);
extern void LUA_CreateSound(lua_State *L);
extern void LUA_CreateConfig(lua_State *L);
extern void LUA_CreateRooms(lua_State *L);
extern void LUA_CreateGame(lua_State *L);
extern void LUA_CreateCreatures(lua_State *L);
extern void LUA_CreateObjects(lua_State *const L);
extern void LUA_CreateAssaultStats(lua_State *const L);

static int M_LoadFile(lua_State *const L, const char *const path)
{
    return luaL_loadfile(L, path);
}

// Shared loader+pcall helper for Eval/EvalFile to capture errors with source
static LUA_RESULT M_LuaLoadAndRun(
    lua_State *const L, int (*const loader)(lua_State *, const char *),
    const char *const src)
{
    LUA_RESULT result = { .code = LUA_OK, .message = nullptr };
    int status = loader(L, src);
    if (status != LUA_OK) {
        result.code = status;
        result.message = Memory_DupStr(lua_tostring(L, -1));
        lua_pop(L, 1);
        return result;
    }
    status = lua_pcall(L, 0, LUA_MULTRET, 0);
    if (status != LUA_OK) {
        result.code = status;
        result.message = Memory_DupStr(lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    return result;
}

// Loader closure for embedded TRX modules, invoked via package.preload.
static int M_TRXEmbeddedModuleLoader(lua_State *const L)
{
    const uint8_t *const data = lua_touserdata(L, lua_upvalueindex(1));
    const size_t size = (size_t)lua_tointeger(L, lua_upvalueindex(2));
    const char *const chunk_name = lua_tostring(L, lua_upvalueindex(3));
    int status = luaL_loadbuffer(L, (const char *)data, size, chunk_name);
    if (status != LUA_OK) {
        lua_error(L);
    }
    status = lua_pcall(L, 0, LUA_MULTRET, 0);
    if (status != LUA_OK) {
        lua_error(L);
    }
    // Return all values pushed by the chunk.
    return lua_gettop(L);
}

static void M_LoadTRXCModule(lua_State *const L, void (*loader)(lua_State *))
{
    LOG_DEBUG("Loading TRXC module %p", loader);
    loader(L);
}

static char *M_DeriveTRXModuleName(const char *path)
{
    char *raw = Memory_DupStr(path);
    size_t raw_len = strlen(raw);

    // Drop ".lua"
    if (raw_len > 4 && strcmp(raw + raw_len - 4, ".lua") == 0) {
        raw[raw_len - 4] = '\0';
    }

    // Convert '/' → '.'
    for (char *c = raw; *c; ++c) {
        if (*c == '/') {
            *c = '.';
        }
    }

    // Prefix "trx."
    const char *modprefix = "trx.";
    size_t prefix_len = strlen(modprefix);
    raw_len = strlen(raw);
    char *name = Memory_Alloc(prefix_len + raw_len + 1);
    memcpy(name, modprefix, prefix_len);
    memcpy(name + prefix_len, raw, raw_len + 1);

    Memory_FreePointer(&raw);
    return name;
}

static void M_RegisterTRXPreloadEmbedded(
    lua_State *const L, const uint8_t *const data, const size_t size,
    const char *const chunk_name, const char *const name)
{
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "preload");
    lua_pushlightuserdata(L, (void *)data);
    lua_pushinteger(L, (lua_Integer)size);
    lua_pushstring(L, chunk_name);
    lua_pushcclosure(L, M_TRXEmbeddedModuleLoader, 3);
    lua_setfield(L, -2, name);
    lua_pop(L, 2);
}

static void M_RequireTRXModule(lua_State *const L, const char *name)
{
    lua_getglobal(L, "require");
    lua_pushstring(L, name);
    if (lua_pcall(L, 1, LUA_MULTRET, 0) != LUA_OK) {
        LOG_ERROR("Failed to require module %s: %s", name, lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_settop(L, 0);
}

static void M_LoadTRXScripts(lua_State *const L)
{
    for (const LUA_EMBEDDED_SCRIPT *script = g_LUA_EmbeddedScripts;
         script->path != nullptr; script++) {
        LOG_DEBUG("Loading TRX module %s", script->path);
        char *name = M_DeriveTRXModuleName(script->path);
        const char *const chunk_name =
            String_FormatStatic("@trx/%s", script->path);
        M_RegisterTRXPreloadEmbedded(
            L, script->data, script->size, chunk_name, name);
        M_RequireTRXModule(L, name);
        Memory_FreePointer(&name);
    }
}

void LUA_Init(void)
{
    lua_State *const L = luaL_newstate();
    ASSERT(L != nullptr);
    luaL_openlibs(L);

    lua_newtable(L);
    lua_setglobal(L, "trxc");
    lua_newtable(L);
    lua_setglobal(L, "trx");

    // Initialize internal modules
    M_LoadTRXCModule(L, LUA_CreateCatalog);
    M_LoadTRXCModule(L, LUA_CreateCamera);
    M_LoadTRXCModule(L, LUA_CreateConsole);
    M_LoadTRXCModule(L, LUA_CreateEvents);
    M_LoadTRXCModule(L, LUA_CreateItems);
    M_LoadTRXCModule(L, LUA_CreateLara);
    M_LoadTRXCModule(L, LUA_CreateLog);
    M_LoadTRXCModule(L, LUA_CreateMusic);
    M_LoadTRXCModule(L, LUA_CreateSound);
    M_LoadTRXCModule(L, LUA_CreateConfig);
    M_LoadTRXCModule(L, LUA_CreateRooms);
    M_LoadTRXCModule(L, LUA_CreateGame);
    M_LoadTRXCModule(L, LUA_CreateCreatures);
    M_LoadTRXCModule(L, LUA_CreateObjects);
    M_LoadTRXCModule(L, LUA_CreateAssaultStats);

    M_PRIV *const p = &m_Priv;
    p->state = L;

    M_LoadTRXScripts(L);
}

void LUA_Shutdown(void)
{
    M_PRIV *const p = &m_Priv;
    Lua_ShutdownEvents();
    if (p->state != nullptr) {
        lua_close(p->state);
        p->state = nullptr;
    }
}

LUA_CONTEXT Lua_GetScriptContext(void)
{
    M_PRIV *const p = &m_Priv;
    return p->context;
}

void Lua_SetScriptContext(const LUA_CONTEXT context)
{
    M_PRIV *const p = &m_Priv;
    p->context = context;
}

LUA_RESULT Lua_Eval(const char *const code)
{
    M_PRIV *const p = &m_Priv;
    return M_LuaLoadAndRun(p->state, luaL_loadstring, code);
}

LUA_RESULT Lua_EvalFile(const char *const path)
{
    M_PRIV *const p = &m_Priv;
    return M_LuaLoadAndRun(p->state, M_LoadFile, path);
}

void Lua_ReloadLevelScript(void)
{
    const GF_LEVEL *const level = GF_GetCurrentLevel();
    if (level == nullptr) {
        return;
    }

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
}

void Lua_FreeResult(LUA_RESULT *const result)
{
    if (result != nullptr) {
        Memory_FreePointer(&result->message);
    }
}
