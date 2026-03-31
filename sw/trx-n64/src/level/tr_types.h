/*
 * TRX-N64: Tomb Raider 1 level data types
 *
 * Lightweight structures matching the TR1 .PHD binary format.
 * Sized for N64's limited RAM — we keep only what's needed for
 * rendering and collision. Audio/cinematic data is streamed.
 *
 * Coordinates: TR1 uses a fixed-point system where 1 sector = 1024 units.
 * Lara is about 762 units tall.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TR_TYPES_H
#define TR_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/* -------------------------------------------------------------------
 * Math primitives
 * ------------------------------------------------------------------- */

typedef struct { int16_t x, y, z; } tr_xyz16_t;
typedef struct { int32_t x, y, z; } tr_xyz32_t;

/* -------------------------------------------------------------------
 * Room vertex (8 bytes in file, 8 bytes in RAM)
 * ------------------------------------------------------------------- */

typedef struct {
    int16_t x, y, z;       /* Position relative to room origin */
    int16_t light;          /* Lighting intensity (0=bright, 0x1FFF=dark) */
} tr_room_vertex_t;

/* -------------------------------------------------------------------
 * Face (10 bytes for quad, 8 bytes for tri)
 * ------------------------------------------------------------------- */

/* Face classification flags (computed once at load) */
#define FACE_FLAG_ESSENTIAL  0x01  /* Boundary/silhouette — never decimate */
#define FACE_FLAG_FLOOR      0x02  /* Horizontal floor surface */
#define FACE_FLAG_CEILING    0x04  /* Horizontal ceiling surface */
#define FACE_FLAG_WALL       0x08  /* Vertical wall at room edge */
#define FACE_FLAG_PORTAL_ADJ 0x10  /* Near a portal opening */
#define FACE_FLAG_DETAIL     0x80  /* Small detail — safe to skip */

typedef struct {
    uint16_t vertices[4];   /* Vertex indices (3rd==4th for triangles) */
    uint16_t texture;       /* Bits 0-14: texture index, bit 15: double-sided */
    uint8_t  vertex_count;  /* 3 or 4 */
    uint8_t  flags;         /* FACE_FLAG_* classification */
} tr_face_t;

/* -------------------------------------------------------------------
 * Portal between rooms
 * ------------------------------------------------------------------- */

typedef struct {
    int16_t  room;          /* Destination room index */
    tr_xyz16_t normal;      /* Surface normal */
    tr_xyz16_t vertices[4]; /* Portal polygon vertices (room-relative) */
} tr_portal_t;

/* -------------------------------------------------------------------
 * Room sector (floor/ceiling cell)
 * Each room is divided into a grid of 1024×1024 sectors.
 * ------------------------------------------------------------------- */

typedef struct {
    uint16_t floor_data_idx;   /* Index into floor data array */
    int16_t  box;              /* Pathfinding box index */
    uint8_t  pit_room;         /* Room below (255 = none) */
    int8_t   floor;            /* Floor height (× 256 = world Y) */
    uint8_t  sky_room;         /* Room above (255 = none) */
    int8_t   ceiling;          /* Ceiling height (× 256 = world Y) */
} tr_sector_t;

/* -------------------------------------------------------------------
 * Room light
 * ------------------------------------------------------------------- */

typedef struct {
    tr_xyz32_t pos;
    int16_t    intensity;
    int32_t    falloff;
} tr_light_t;

/* -------------------------------------------------------------------
 * Room static mesh reference
 * ------------------------------------------------------------------- */

typedef struct {
    tr_xyz32_t pos;
    int16_t    rotation;        /* Y rotation (angle units) */
    int16_t    shade;
    int16_t    static_mesh_id;
} tr_room_static_mesh_t;

/* -------------------------------------------------------------------
 * Room mesh — geometry for one room
 * ------------------------------------------------------------------- */

typedef struct {
    tr_room_vertex_t *vertices;
    int16_t           vertex_count;

    tr_face_t        *faces;        /* All faces (quads + tris merged) */
    int16_t           face_count;

    /* Sprite billboards in room */
    uint16_t         *sprite_vertices; /* Vertex indices */
    uint16_t         *sprite_textures; /* Sprite texture indices */
    int16_t           sprite_count;
} tr_room_mesh_t;

/* -------------------------------------------------------------------
 * Room — the fundamental spatial unit
 * ------------------------------------------------------------------- */

typedef struct {
    /* World position */
    int32_t x, z;
    int32_t min_floor, max_ceiling;

    /* Mesh data */
    tr_room_mesh_t mesh;

    /* Portal connectivity */
    tr_portal_t *portals;
    int16_t      portal_count;

    /* Sector grid */
    tr_sector_t *sectors;
    int16_t      sectors_x, sectors_z;

    /* Lighting */
    tr_light_t  *lights;
    int16_t      light_count;
    int16_t      ambient;       /* Ambient light level */

    /* Static meshes */
    tr_room_static_mesh_t *static_meshes;
    int16_t                static_mesh_count;

    /* Room properties */
    int16_t  flipped_room;      /* Alternate room index (-1 = none) */
    uint16_t flags;             /* Bit 0: underwater, etc. */
} tr_room_t;

/* -------------------------------------------------------------------
 * Object texture (maps face UV coords to a texture page tile)
 * ------------------------------------------------------------------- */

typedef struct {
    uint16_t draw_type;         /* DRAW_OPAQUE, DRAW_BLEND, etc. */
    uint16_t tex_page;          /* Texture page index */
    struct {
        uint16_t u, v;
    } uv[4];                    /* UV coordinates (8.8 fixed point) */
} tr_object_texture_t;

/* -------------------------------------------------------------------
 * Object mesh (for entities like Lara, enemies, pickups)
 * ------------------------------------------------------------------- */

typedef struct {
    tr_xyz16_t center;
    int16_t    radius;

    tr_xyz16_t *vertices;
    int16_t     vertex_count;

    /* Lighting: normals or pre-computed light values */
    int16_t    *lights;     /* If num_lights < 0: per-vertex light values */
    tr_xyz16_t *normals;    /* If num_lights > 0: per-vertex normals */
    int16_t     num_lights; /* Sign determines which array is valid */

    tr_face_t  *faces;
    int16_t     face_count;
    int16_t     textured_face_count;  /* First N faces are textured, rest are flat-colored */
} tr_object_mesh_t;

/* -------------------------------------------------------------------
 * Animation structures
 * ------------------------------------------------------------------- */

typedef struct {
    uint32_t frame_offset;
    uint8_t  interpolation;
    uint8_t  frame_size;
    int16_t  state_id;
    int32_t  velocity;
    int32_t  acceleration;
    int16_t  frame_base;
    int16_t  frame_end;
    int16_t  jump_anim;
    int16_t  jump_frame;
    int16_t  num_changes;
    int16_t  change_idx;
    int16_t  num_commands;
    int16_t  command_idx;
} tr_anim_t;

typedef struct {
    int16_t goal_state;
    int16_t num_ranges;
    int16_t range_idx;
} tr_anim_change_t;

typedef struct {
    int16_t start_frame;
    int16_t end_frame;
    int16_t link_anim;
    int16_t link_frame;
} tr_anim_range_t;

typedef struct {
    int32_t flags;  /* Bit 0: pop, bit 1: push */
    int32_t x, y, z;
} tr_anim_bone_t;

/* -------------------------------------------------------------------
 * Game object definition (template for entity types)
 * ------------------------------------------------------------------- */

typedef struct {
    int32_t  object_id;
    int16_t  mesh_count;
    int16_t  mesh_idx;
    int32_t  bone_idx;
    uint32_t frame_offset;
    int16_t  anim_idx;
} tr_object_def_t;

/* -------------------------------------------------------------------
 * Static object definition
 * ------------------------------------------------------------------- */

typedef struct {
    int32_t  object_id;
    int16_t  mesh_idx;
    tr_xyz16_t draw_min, draw_max;
    tr_xyz16_t coll_min, coll_max;
    uint16_t flags;             /* Bit 0: collidable, bit 1: visible */
} tr_static_object_t;

/* -------------------------------------------------------------------
 * Placed item (entity instance in a room)
 * ------------------------------------------------------------------- */

typedef struct {
    int16_t    object_id;
    int16_t    room;
    tr_xyz32_t pos;
    int16_t    rotation;    /* Y rotation */
    int16_t    shade;
    uint16_t   flags;
} tr_item_t;

/* -------------------------------------------------------------------
 * Sprite texture (for billboard sprites)
 * ------------------------------------------------------------------- */

typedef struct {
    uint16_t tex_page;
    uint16_t offset;
    uint16_t width, height;
    int16_t  x0, y0, x1, y1;
} tr_sprite_texture_t;

/* -------------------------------------------------------------------
 * Sprite sequence (animation frames for a sprite type)
 * ------------------------------------------------------------------- */

typedef struct {
    int32_t  object_id;
    int16_t  frame_count;   /* Can be negative (negate for actual count) */
    int16_t  texture_idx;   /* Index into sprite textures */
} tr_sprite_sequence_t;

/* -------------------------------------------------------------------
 * Complete level data
 * ------------------------------------------------------------------- */

typedef struct {
    /* Rooms */
    tr_room_t          *rooms;
    int16_t             room_count;

    /* Floor data (shared across all rooms) */
    int16_t            *floor_data;
    int32_t             floor_data_size;

    /* Object meshes */
    tr_object_mesh_t   *meshes;
    int32_t             mesh_count;

    /* Animations */
    tr_anim_t          *anims;
    int32_t             anim_count;
    tr_anim_change_t   *anim_changes;
    int32_t             anim_change_count;
    tr_anim_range_t    *anim_ranges;
    int32_t             anim_range_count;
    int16_t            *anim_commands;
    int32_t             anim_command_count;
    tr_anim_bone_t     *anim_bones;
    int32_t             anim_bone_count;
    int16_t            *anim_frames;
    int32_t             anim_frame_count;

    /* Object definitions */
    tr_object_def_t    *objects;
    int32_t             object_count;

    /* Static objects */
    tr_static_object_t *static_objects;
    int32_t             static_object_count;

    /* Object textures */
    tr_object_texture_t *object_textures;
    int32_t              object_texture_count;

    /* Sprite textures and sequences */
    tr_sprite_texture_t  *sprite_textures;
    int32_t               sprite_texture_count;
    tr_sprite_sequence_t *sprite_sequences;
    int32_t               sprite_sequence_count;

    /* Items (placed entities) */
    tr_item_t          *items;
    int32_t             item_count;

    /* Texture pages (8-bit palettized, 256×256 each) */
    uint8_t            *tex_pages;
    int32_t             tex_page_count;

    /* Palette (256 × RGB) */
    uint8_t             palette[256 * 3];

    /* Light map (32 × 256 lookup table) */
    uint8_t             light_map[32 * 256];

    /* Sound data */
    int16_t             sound_map[256];      /* Effect ID → sample info index */
    int16_t            *sound_details;       /* Sample info entries */
    int32_t             sound_detail_count;
    int32_t            *sample_indices;       /* Byte offsets into sample data */
    int32_t             sample_index_count;
    uint8_t            *sample_data;          /* Raw PCM sample data */
    int32_t             sample_data_size;
} tr_level_t;

/* -------------------------------------------------------------------
 * Limits for N64 memory budget
 * ------------------------------------------------------------------- */

#define TR_MAX_ROOMS            256
#define TR_MAX_ITEMS            512
#define TR_MAX_MESHES           512
#define TR_MAX_OBJECT_TEXTURES  4096
#define TR_MAX_SPRITE_TEXTURES  512
#define TR_MAX_ANIMS            512
#define TR_MAX_OBJECTS          256
#define TR_MAX_STATIC_OBJECTS   64

#endif /* TR_TYPES_H */
