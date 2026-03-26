#pragma once

#include <lualib.h>
#include <stdint.h>

// Result of evaluating a Lua chunk.
typedef struct {
    int32_t code; // LUA_OK, LUA_ERRSYNTAX, LUA_ERRRUN, etc.
    char *message; // Error text (nullptr if code == LUA_OK).
} LUA_RESULT;

typedef enum {
    LUA_CONTEXT_GLOBAL,
    LUA_CONTEXT_LEVEL,
} LUA_CONTEXT;

void LUA_Init(void);
void LUA_Shutdown(void);

// Set script context: level script vs global script
LUA_CONTEXT Lua_GetScriptContext(void);
void Lua_SetScriptContext(LUA_CONTEXT context);

// Evaluate a Lua code string. Caller must free the result with Lua_FreeResult.
LUA_RESULT Lua_Eval(const char *code);

// Free the LUA eval result.
void Lua_FreeResult(LUA_RESULT *result);

// Evaluate a Lua script file. Caller must free the result with Lua_FreeResult.
LUA_RESULT Lua_EvalFile(const char *path);

// Reload current level script and reset level-scoped listeners.
void Lua_ReloadLevelScript(void);
