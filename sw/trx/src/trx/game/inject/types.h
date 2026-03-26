#pragma once

#include <trx/core/math.h>
#include <trx/core/virtual_file.h>
#include <trx/game/inject/enum.h>
#include <trx/game/objects/ids.h>

typedef struct {
    INJECTION_MODE mode;
} INJECTION_CONTEXT;

typedef struct {
    char *path;
    VFILE *fp;
    INJECTION_VERSION version;
    INJECTION_FILE_TYPE type;
    bool relevant;
} INJECTION;

typedef struct {
    const INJECTION *injection;
    INJECTION_CHUNK_TYPE type;
    int32_t num_blocks;
    int32_t total_size;
} INJECTION_CHUNK;

typedef struct {
    int16_t room_index;
    int16_t num_vertices;
    int16_t num_quads;
    int16_t num_triangles;
    int16_t num_static_2ds;
    int16_t num_static_3ds;
} INJECTION_MESH_META;

typedef struct {
    INJECT_OBJECT_TYPE type;
    int32_t id;
} INJECTION_OBJECT_INFO;
