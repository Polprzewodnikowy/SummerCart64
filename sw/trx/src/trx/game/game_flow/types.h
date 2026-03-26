#pragma once

#include <trx/game/fx/weather.h>
#include <trx/game/game_flow/enum.h>
#include <trx/game/music/ids.h>
#include <trx/game/objects/types.h>

#include <stdint.h>

typedef struct GF_COMMAND {
    GF_ACTION action;
    int32_t param;
} GF_COMMAND;

// ----------------------------------------------------------------------------
// Sequencer structures
// ----------------------------------------------------------------------------

typedef struct {
    GF_SEQUENCE_EVENT_TYPE type;
    void *data;
} GF_SEQUENCE_EVENT;

typedef struct {
    int32_t length;
    GF_SEQUENCE_EVENT *events;
} GF_SEQUENCE;

// Concrete events data

typedef struct {
    char *path;
    bool is_legal;
    bool is_credit;
    float display_time;
    float fade_in_time;
    float fade_out_time;
} GF_DISPLAY_PICTURE_DATA;

typedef struct {
    char *image_path;
} GF_GLOBE_SELECT_DATA;

typedef enum {
    GF_INV_REGULAR,
    GF_INV_SECRET,
} GF_INV_TYPE;

typedef struct {
    OBJECT_ID object_id;
    GF_INV_TYPE inv_type;
    int32_t quantity;
} GF_ADD_ITEM_DATA;

// ----------------------------------------------------------------------------
// Game flow level structures
// ----------------------------------------------------------------------------

typedef struct {
    int32_t count;
    char **data_paths;
} INJECTION_DATA;

typedef struct {
    const char *path;
    bool is_legal;
    bool is_credit;
} GF_FMV;

typedef struct {
    bool is_present;
    int32_t count;
    MUSIC_ID *ids;
} GF_AMBIENT_DATA;

typedef struct {
    struct {
        bool is_present;
        float value;
    } fog_start, fog_end;
    struct {
        bool is_present;
        bool value;
    } fog_transparency;
    struct {
        bool is_present;
        RGB_888 value;
    } fog_color;
    struct {
        bool is_present;
        RGB_888 value;
    } water_color;
    char *sfx_path;
    struct {
        bool is_present;
        bool value;
    } cold_water;
    struct {
        bool is_present;
        int32_t value;
    } death_tile;
} GF_LEVEL_SETTINGS;

typedef struct {
    int32_t enemy_num;
    int32_t count;
    int16_t *object_ids;
} GF_DROP_ITEM_DATA;

typedef struct {
    int32_t num;
    GF_LEVEL_TYPE type;
    char *path;
    char *title;
    // Path to a Lua script executed when this level loads
    char *script_path;

    MUSIC_ID music_track;
    char *lara_outfit;
    GF_SEQUENCE sequence;
    INJECTION_DATA injections;

    GF_LEVEL_SETTINGS settings;
    WEATHER_TYPE weather_type;
    bool water_particles;

    struct {
        uint32_t pickups;
        uint32_t kills;
        uint32_t ally_kills;
        uint32_t secrets;
    } unobtainable;

    struct {
        int32_t count;
        GF_DROP_ITEM_DATA *data;
    } item_drops;
} GF_LEVEL;

typedef struct {
    int32_t count;
    GF_LEVEL *levels;
} GF_LEVEL_TABLE;

typedef struct {
    XYZ_16 rot;
    int32_t start_level_ordinal;
    int32_t completion_level_ordinal;
    uint32_t prereq_mask;
    uint8_t mesh_idx;
} GF_GLOBE_ENTRY;

// ----------------------------------------------------------------------------
// Mod metadata
// ----------------------------------------------------------------------------

typedef struct {
    char *name;
    int32_t engine;
    char *extends;
} GF_MOD_META;

// ----------------------------------------------------------------------------
// Game flow structures
// ----------------------------------------------------------------------------

typedef struct {
    char *path;

    GF_MOD_META meta;

    GF_LEVEL *title_level;
    GF_LEVEL_TABLE level_tables[GFLT_NUMBER_OF];

    // FMVs
    struct {
        int32_t fmv_count;
        GF_FMV *fmvs;
    };

    // savegame settings
    struct {
        char *savegame_file_fmt;
    };

    // global settings
    struct {
        char *main_menu_background_path;
        bool enable_tr2_item_drops;
        bool convert_dropped_guns;
        GF_AMBIENT_DATA ambient_tracks;
    };

    // other data
    GF_LEVEL_SETTINGS settings;
    INJECTION_DATA injections;

    // Globe select entries
    struct {
        int32_t count;
        GF_GLOBE_ENTRY *entries;
    } globe;

    // Path to a global Lua script executed after game initialization
    char *main_script_path;
} GAME_FLOW;
