#pragma once

#include <stddef.h>

// Internal game memory manager using an arena allocator. Memory is allocated
// in discrete chunks, with each allocation request served via pointer
// arithmetic within the active chunk. When a request exceeds the current
// chunk's capacity, a new chunk is allocated to continue servicing
// allocations. This design offers very fast allocation speeds, but individual
// blocks cannot be freed – only the entire arena can be reset when needed. For
// more granular memory management, use Memory_Alloc / Memory_Free.

typedef enum {
    // clang-format off
    GBUF_TEXTURE_PAGES,
    GBUF_PALETTES,
    GBUF_OBJECT_TEXTURES,
    GBUF_SPRITE_TEXTURES,
    GBUF_STATIC_OBJECTS_3D,
    GBUF_STATIC_OBJECTS_2D,
    GBUF_MESH_POINTERS,
    GBUF_MESHES,
    GBUF_ANIMS,
    GBUF_ANIM_CHANGES,
    GBUF_ANIM_RANGES,
    GBUF_ANIM_COMMANDS,
    GBUF_ANIM_BONES,
    GBUF_ANIM_FRAMES,
    GBUF_ROOMS,
    GBUF_ROOM_MESH,
    GBUF_ROOM_PORTALS,
    GBUF_ROOM_SECTORS,
    GBUF_ROOM_LIGHTS,
    GBUF_ROOM_STATIC_MESHES,
    GBUF_FLOOR_DATA,
    GBUF_OUTSIDE_ROOM_TABLE,
    GBUF_ITEMS,
    GBUF_ITEM_DATA,
    GBUF_EFFECTS,
    GBUF_CAMERAS,
    GBUF_SOUND_SOURCES,
    GBUF_BOXES,
    GBUF_OVERLAPS,
    GBUF_GROUND_ZONE,
    GBUF_FLY_ZONE,
    GBUF_ANIMATED_TEXTURE_RANGES,
    GBUF_CINEMATIC_FRAMES,
    GBUF_CREATURE_DATA,
    GBUF_CREATURE_LOT,
    GBUF_SAMPLE_INFOS,
    GBUF_SAMPLES,
    GBUF_DEMO_BUFFER,
    GBUF_WALKABLES,
    GBUF_NUM_MALLOC_TYPES,
    // clang-format on
} GAME_BUFFER;

void GameBuf_Init(void);
void GameBuf_Shutdown(void);
void GameBuf_ResetSingle(GAME_BUFFER buffer);
void GameBuf_Reset(void);

void *GameBuf_Alloc(size_t alloc_size, GAME_BUFFER buffer);
