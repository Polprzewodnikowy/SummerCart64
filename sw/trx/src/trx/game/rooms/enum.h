#pragma once

typedef enum {
    HT_WALL = 0,
    HT_SMALL_SLOPE = 1,
    HT_BIG_SLOPE = 2,
    HT_DIAGONAL = 3,
    HT_SPLIT_TRI = 4,
} HEIGHT_TYPE;

typedef enum {
    RLM_NORMAL = 0,
    RLM_FLICKER = 1,
    RLM_GLOW = 2,
    RLM_SUNSET = 3,
    RLM_NUMBER_OF = 4,
} ROOM_LIGHT_MODE;

typedef enum {
    RFS_NONE = 0,
    RFS_UNFLIPPED = 1,
    RFS_FLIPPED = 2,
} ROOM_FLIP_STATUS;

typedef enum {
    FT_FLOOR = 0,
    FT_DOOR = 1,
    FT_TILT = 2,
    FT_ROOF = 3,
    FT_TRIGGER = 4,
    FT_LAVA = 5,
    FT_CLIMB = 6,
    FT_FLOOR_NWSE_SOLID = 7,
    FT_FLOOR_NESW_SOLID = 8,
    FT_ROOF_NWSE_SOLID = 9,
    FT_ROOF_NESW_SOLID = 10,
    FT_FLOOR_NWSE_PORTAL_SW = 11,
    FT_FLOOR_NWSE_PORTAL_NE = 12,
    FT_FLOOR_NESW_PORTAL_SE = 13,
    FT_FLOOR_NESW_PORTAL_NW = 14,
    FT_ROOF_NWSE_PORTAL_SW = 15,
    FT_ROOF_NWSE_PORTAL_NE = 16,
    FT_ROOF_NESW_PORTAL_NW = 17,
    FT_ROOF_NESW_PORTAL_SE = 18,
    FT_MONKEY = 19,
} FLOOR_TYPE;

typedef enum {
    SPLIT_NONE,
    SPLIT_NWSE_SOLID,
    SPLIT_NESW_SOLID,
    SPLIT_NWSE_PORTAL_SW,
    SPLIT_NWSE_PORTAL_NE,
    SPLIT_NESW_PORTAL_SE,
    SPLIT_NESW_PORTAL_NW,
} SPLIT_TYPE;

typedef enum {
    SURFACE_FLOOR,
    SURFACE_CEILING,
} SURFACE_TYPE;

typedef enum {
    TO_OBJECT = 0,
    TO_CAMERA = 1,
    TO_SINK = 2,
    TO_FLIPMAP = 3,
    TO_FLIPON = 4,
    TO_FLIPOFF = 5,
    TO_TARGET = 6,
    TO_FINISH = 7,
    TO_CD = 8,
    TO_FLIPEFFECT = 9,
    TO_SECRET = 10,
    TO_BODY_BAG = 11,
} TRIGGER_OBJECT;

typedef enum {
    TT_TRIGGER = 0,
    TT_PAD = 1,
    TT_SWITCH = 2,
    TT_KEY = 3,
    TT_PICKUP = 4,
    TT_HEAVY = 5,
    TT_ANTIPAD = 6,
    TT_COMBAT = 7,
    TT_DUMMY = 8,
    TT_ANTITRIGGER = 9,
} TRIGGER_TYPE;

typedef enum {
    LADDER_NONE = 0,
    LADDER_NORTH = 1 << 0,
    LADDER_EAST = 1 << 1,
    LADDER_SOUTH = 1 << 2,
    LADDER_WEST = 1 << 3,
    LADDER_CEILING = 1 << 4,
} LADDER_DIRECTION;
