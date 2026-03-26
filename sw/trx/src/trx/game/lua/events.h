// Lua event listener support
#pragma once

#include <lualib.h>
#include <stdint.h>

// Event types for Lua listeners
typedef enum {
    LUA_EVENT_BEFORE_LEVEL_FILE,
    LUA_EVENT_AFTER_LEVEL_FILE,
    LUA_EVENT_AFTER_LEVEL_STATE,
    LUA_EVENT_GAME_START,
    LUA_EVENT_PICKUP,
    LUA_EVENT_BEFORE_CONTROL,
    LUA_EVENT_AFTER_CONTROL,
} LUA_EVENT_TYPE;

typedef enum {
    LUA_EVENT_ARG_NIL,
    LUA_EVENT_ARG_INT32,
    LUA_EVENT_ARG_BOOL,
    LUA_EVENT_ARG_NUMBER,
    LUA_EVENT_ARG_STRING,
} LUA_EVENT_ARG_TYPE;

typedef struct {
    LUA_EVENT_ARG_TYPE type;
    union {
        int32_t i32;
        bool b;
        double number;
        const char *str;
    } value;
} LUA_EVENT_ARG;

// Initialize event API in Lua state
void LUA_CreateEvents(lua_State *L);
void Lua_ShutdownEvents(void);

// Clear all listeners declared during the current level script
void Lua_ClearLevelListeners(void);

// Fire a Lua event of given type with arbitrary arguments
void Lua_FireEventEx(
    LUA_EVENT_TYPE ev, const LUA_EVENT_ARG *args, int32_t arg_count);

// Fire a Lua event of given type with int32 argument
void Lua_FireEventInt32(LUA_EVENT_TYPE ev, int32_t arg);
