#pragma once

#include <trx/core/colors.h>
#include <trx/core/math.h>
#include <trx/game/items/const.h>
#include <trx/game/rooms/enum.h>
#include <trx/game/types.h>

#define ROOM_DRAWSET_WORDS (MAX_ITEMS / 64)

typedef struct TRIGGER_CMD {
    TRIGGER_OBJECT type;
    void *parameter;
    struct TRIGGER_CMD *next_cmd;
} TRIGGER_CMD;

typedef struct {
    int16_t camera_num;
    uint8_t timer;
    uint8_t glide;
    bool one_shot;
} TRIGGER_CAMERA_DATA;

typedef struct {
    bool enabled;
    TRIGGER_TYPE type;
    int8_t timer;
    int16_t mask;
    bool one_shot;
    int16_t item_index;
    TRIGGER_CMD *command;
} TRIGGER;

typedef struct {
    int16_t room_num;
    XYZ_16 normal;
    XYZ_16 vertex[4];
    BOUNDS_32 bounds;
} PORTAL;

typedef struct {
    uint16_t count;
    PORTAL portal[];
} PORTALS;

typedef struct WALKABLE {
    int16_t item_num;
    XYZ_32 pos;
    struct WALKABLE *next;
} WALKABLE;

typedef struct {
    SPLIT_TYPE type;
    int16_t tilts[4];
    int32_t h1;
    int32_t h2;
} SPLIT;

typedef struct {
    SURFACE_TYPE type;
    int32_t height;
    bool is_split;
    union {
        int16_t tilt;
        SPLIT split;
    };
} SURFACE;

typedef struct {
    uint16_t idx;
    int16_t box;
    bool is_death_sector;
    LADDER_DIRECTION ladder;
    TRIGGER *trigger;
    WALKABLE *walkable;
    struct {
        int16_t pit;
        int16_t sky;
        int16_t wall;
    } portal_room;
    SURFACE floor;
    SURFACE ceiling;
    uint8_t fx;
    bool stopper;
} SECTOR;

typedef struct {
    XYZ_32 pos;
    SHADE shade;
    FALLOFF falloff;
    RGB_888 color;
    uint8_t type; // TR3: 0 = point, != 0 = sun
    XYZ_16 dir; // TR3: sun direction (type != 0)
} LIGHT;

typedef struct {
    XYZ_16 pos;
    RGBA_8888 color;
    int16_t light_base;
    uint8_t light_table_value;
    struct {
        bool disable_wibble;
        bool move;
        bool glow;
    } flags;
} ROOM_VERTEX;

typedef struct {
    uint16_t texture;
    uint16_t vertex;
} ROOM_SPRITE;

typedef struct {
    int16_t num_vertices;
    struct {
        int16_t count;
        FACE *data;
    } all_faces, face4s, face3s;
    struct {
        int16_t count;
        ROOM_SPRITE *data;
    } sprites;
    ROOM_VERTEX *vertices;
} ROOM_MESH;

typedef struct {
    XYZ_32 pos;
    struct {
        int16_t y;
    } rot;
    RGBA_8888 color;
    SHADE shade;
    int16_t static_num;
} STATIC_MESH;

typedef struct {
    uint64_t bits[ROOM_DRAWSET_WORDS];
    uint16_t count;
} ROOM_DRAWSET;

typedef struct {
    ROOM_MESH mesh;
    PORTALS *portals;
    SECTOR *sectors;
    LIGHT *lights;
    STATIC_MESH *static_meshes;
    XYZ_32 pos;
    int32_t min_floor;
    int32_t max_ceiling;
    struct {
        int16_t z;
        int16_t x;
    } size;
    int16_t ambient;
    ROOM_LIGHT_MODE light_mode;
    int16_t num_lights;
    int16_t num_static_meshes;
    int16_t item_num;
    int16_t effect_num;
    int16_t flipped_room;
    ROOM_FLIP_STATUS flip_status;
    struct {
        bool underwater;
        bool outside;
        bool wind;
        bool inside;
        bool dynamic_lit;
        bool swamp;
    } flags;

    ROOM_DRAWSET drawn_items;
    uint8_t water_scheme;
    uint8_t reverb_info;
} ROOM;
