#pragma once

// clang-format off
typedef enum {
    INJECTION_MODE_STATS,
    INJECTION_MODE_FULL,
} INJECTION_MODE;

typedef enum {
    INJ_VERSION_1 = 1,
    INJ_VERSION_2 = 2,
    INJ_VERSION_3 = 3,
    INJ_VERSION_4 = 4,
    INJ_VERSION_5 = 5,
    INJ_VERSION_6 = 6,
} INJECTION_VERSION;

typedef enum {
    IFT_GENERAL           = 0,
    IFT_BRAID             = 1,
    IFT_TEXTURE_FIX       = 2,
    IFT_PS1_SFX           = 3,
    IFT_FLOOR_DATA        = 4,
    IFT_LARA_ANIMS        = 5,
    IFT_ITEM_POSITION     = 6,
    IFT_PS1_ENEMY         = 7,
    IFT_ALTER_ANIM_SPRITE = 8,
    IFT_SKYBOX            = 9,
    IFT_PS1_CRYSTAL       = 10,
    IFT_NUMBER_OF         = 11,
} INJECTION_FILE_TYPE;

typedef enum {
    ITT_ITEM_META  = 0,
    ITT_ROOM_COUNT = 1,
    ITT_ROOM_META  = 2,
    ITT_NUMBER_OF  = 3,
} INJECTION_TEST_TYPE;

typedef enum {
    ICT_TEXTURE_DATA   = 0,
    ICT_TEXTURE_INFO   = 1,
    ICT_MESH_DATA      = 2,
    ICT_ANIMATION_DATA = 3,
    ICT_OBJECT_DATA    = 4,
    ICT_SFX_DATA       = 5,
    ICT_DATA_EDITS     = 6,
    ICT_CAMERA_DATA    = 7,
    ICT_NUMBER_OF      = 8,
} INJECTION_CHUNK_TYPE;

typedef enum {
    IDT_PALETTE          = 0,
    IDT_TEXTURE_PAGES    = 1,
    IDT_OBJECT_TEXTURES  = 2,
    IDT_SPRITE_TEXTURES  = 3,
    IDT_SPRITE_SEQUENCES = 4,
    IDT_OBJECT_MESHES    = 5,
    IDT_MESH_POINTERS    = 6,
    IDT_ANIM_CHANGES     = 7,
    IDT_ANIM_RANGES      = 8,
    IDT_ANIM_COMMANDS    = 9,
    IDT_ANIM_BONES       = 10,
    IDT_ANIM_FRAMES      = 11,
    IDT_ANIMS            = 12,
    IDT_OBJECTS          = 13,
    IDT_SAMPLE_INFOS     = 14,
    IDT_SAMPLE_INDICES   = 15,
    IDT_SAMPLE_DATA      = 16,
    IDT_FLOOR_EDITS      = 17,
    IDT_ITEM_POS_EDITS   = 18,
    IDT_MESH_EDITS       = 19,
    IDT_TEXTURE_EDITS    = 20,
    IDT_ROOM_EDIT_META   = 21,
    IDT_ROOM_EDITS       = 22,
    IDT_VIS_PORTAL_EDITS = 23,
    IDT_CAMERA_EDITS     = 24,
    IDT_FRAME_EDITS      = 25,
    IDT_OBJECT_3D_EDITS  = 26,
    IDT_ANIM_CMD_EDITS   = 27,
    IDT_SPRITE_EDITS     = 28,
    IDT_STATIC_OBJECTS   = 29,
    IDT_CINEMATIC_FRAMES = 30,
    IDT_OBJ_TYPE_EDITS   = 31,
    IDT_FRAME_REPLACE    = 32,
    IDT_ITEM_FLAG_EDITS  = 33,
    IDT_ANIM_EDITS       = 34,
    IDT_NUMBER_OF        = 35,
} INJECTION_DATA_TYPE;

typedef enum {
    FT_TEXTURED_QUAD     = 0,
    FT_TEXTURED_TRIANGLE = 1,
    FT_COLOURED_QUAD     = 2,
    FT_COLOURED_TRIANGLE = 3,
} FACE_TYPE;

typedef enum {
    FET_TRIGGER_PARAM     = 0,
    FET_MUSIC_ONESHOT     = 1,
    FET_FD_INSERT         = 2,
    FET_ROOM_SHIFT        = 3,
    FET_TRIGGER_ITEM      = 4,
    FET_ROOM_PROPERTIES   = 5,
    FET_TRIGGER_TYPE      = 6,
    FET_SECTOR_OVERWRITE  = 7,
    FET_GLIDE_CAMERA      = 8,
    FET_ZONE_FIX          = 9,
    FET_PORTALS           = 10,
    FET_CLIMB             = 11,
    FET_DELETE_TRIGGER    = 12,
    FET_TRIANGULATE       = 13,
} FLOOR_EDIT_TYPE;

typedef enum {
    RMET_TEXTURE_FACE   = 0,
    RMET_MOVE_FACE      = 1,
    RMET_ALTER_VERTEX   = 2,
    RMET_ROTATE_FACE    = 3,
    RMET_ADD_FACE       = 4,
    RMET_ADD_VERTEX     = 5,
    RMET_ADD_STATIC_2D  = 6,
    RMET_ADD_STATIC_3D  = 7,
    RMET_EDIT_STATIC_3D = 8,
    RMET_VERTEX_FLAGS   = 9,
} ROOM_MESH_EDIT_TYPE;
// clang-format on

typedef enum {
    OBJ_TYPE_OBJECT = 0,
    OBJ_TYPE_STATIC2D = 1,
    OBJ_TYPE_STATIC3D = 2,
} INJECT_OBJECT_TYPE;
