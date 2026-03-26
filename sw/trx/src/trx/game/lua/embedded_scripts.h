#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *path;
    const uint8_t *data;
    size_t size;
} LUA_EMBEDDED_SCRIPT;

extern const LUA_EMBEDDED_SCRIPT g_LUA_EmbeddedScripts[];
