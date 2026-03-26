#pragma once

#include <trx/core/colors.h>
#include <trx/core/memory.h>
#include <trx/core/vector.h>
#include <trx/game/matrix.h>
#include <trx/game/output/textures.h>

typedef struct {
    // attribute 2
    OUTPUT_UVW uvw;
    // attribute 3
    OUTPUT_TEXTURE_SIZE texture_size;
    // attribute 4
    float trapezoid_ratio[2];
} OUTPUT_MESH_TEXTURE;

typedef struct {
    XYZW_F pos;
    XYZ_F normal;
    uint16_t flags;
    int32_t uvw_idx;
    float trapezoid_ratio[2];
    int16_t shade;
    RGBA_8888 color;
    uint8_t light_table_idx;
} OUTPUT_MESH_VERTEX;

// Describes a contiguous block of vertices belonging to one face,
// with sort keys.
typedef struct {
    int32_t vertex_count;
    int32_t *vertex_indices;
    XYZ_F mesh_centroid;
} OUTPUT_MESH_FACE;

typedef struct {
    VECTOR *vertices;

    MEMORY_ARENA_ALLOCATOR allocator;
    int32_t sealed;
    VECTOR *animated_vertices; // OUTPUT_VERTEX_RANGE
    VECTOR *transparent_faces; // OUTPUT_MESH_FACE
    VECTOR *opaque_vertex_indices; // uint32_t
    VECTOR *blend_add_vertex_indices; // uint32_t
} OUTPUT_MESH;

OUTPUT_MESH *Output_Mesh_Create(void);
void Output_Mesh_Destroy(OUTPUT_MESH *mesh);
