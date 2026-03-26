/*
 * MW-N64: Morrowind ESM/ESP file parser
 *
 * Reads Bethesda's TES3 plugin format used by Morrowind.
 * Records are tagged key-value pairs with nested subrecords.
 *
 * Binary format (all little-endian):
 *   Record: [4B name][4B size][4B unused][4B flags][...subrecords]
 *   Subrecord: [4B name][4B size][...data]
 *
 * We parse only the records needed for gameplay:
 *   TES3 - Header (version, master files)
 *   CELL - Interior/exterior cell definitions + object placements
 *   LAND - Terrain heightmaps (65x65 per exterior cell)
 *   STAT - Static mesh references
 *   DOOR - Door definitions
 *   NPC_ - NPCs (stats, dialogue, inventory)
 *   CREA - Creatures
 *   CONT - Containers
 *   LIGH - Lights
 *   MISC/WEAP/ARMO/CLOT/BOOK/INGR/ALCH - Items
 *   DIAL/INFO - Dialogue topics and responses
 *   SCPT - Scripts
 *   GMST - Game settings (constants)
 *
 * All other record types are skipped (their size field lets us jump past).
 *
 * Reference: OpenMW components/esm3/ (C++, used as format documentation)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MW_ESM_H
#define MW_ESM_H

#include <stdint.h>
#include <stdbool.h>

/* -------------------------------------------------------------------
 * ESM reader state
 * ------------------------------------------------------------------- */

typedef struct {
    const uint8_t *data;    /* Pointer to ESM file in memory */
    uint32_t       size;    /* Total file size */
    uint32_t       pos;     /* Current read position */

    /* Current record context */
    uint32_t rec_name;      /* 4-char record name as uint32 */
    uint32_t rec_size;      /* Record data size */
    uint32_t rec_flags;     /* Record flags */
    uint32_t rec_end;       /* Position where record ends */

    /* Current subrecord context */
    uint32_t sub_name;      /* 4-char subrecord name as uint32 */
    uint32_t sub_size;      /* Subrecord data size */
    uint32_t sub_end;       /* Position where subrecord ends */
} esm_reader_t;

/* Pack 4 ASCII chars into a uint32 for fast comparison */
#define ESM_TAG(a,b,c,d) ((uint32_t)(a) | ((uint32_t)(b)<<8) | ((uint32_t)(c)<<16) | ((uint32_t)(d)<<24))

/* Record type tags */
#define ESM_TES3  ESM_TAG('T','E','S','3')
#define ESM_CELL  ESM_TAG('C','E','L','L')
#define ESM_LAND  ESM_TAG('L','A','N','D')
#define ESM_STAT  ESM_TAG('S','T','A','T')
#define ESM_DOOR  ESM_TAG('D','O','O','R')
#define ESM_NPC_  ESM_TAG('N','P','C','_')
#define ESM_CREA  ESM_TAG('C','R','E','A')
#define ESM_CONT  ESM_TAG('C','O','N','T')
#define ESM_LIGH  ESM_TAG('L','I','G','H')
#define ESM_MISC  ESM_TAG('M','I','S','C')
#define ESM_WEAP  ESM_TAG('W','E','A','P')
#define ESM_ARMO  ESM_TAG('A','R','M','O')
#define ESM_CLOT  ESM_TAG('C','L','O','T')
#define ESM_BOOK  ESM_TAG('B','O','O','K')
#define ESM_INGR  ESM_TAG('I','N','G','R')
#define ESM_ALCH  ESM_TAG('A','L','C','H')
#define ESM_DIAL  ESM_TAG('D','I','A','L')
#define ESM_INFO  ESM_TAG('I','N','F','O')
#define ESM_SCPT  ESM_TAG('S','C','P','T')
#define ESM_GMST  ESM_TAG('G','M','S','T')
#define ESM_GLOB  ESM_TAG('G','L','O','B')
#define ESM_SPEL  ESM_TAG('S','P','E','L')
#define ESM_ENCH  ESM_TAG('E','N','C','H')
#define ESM_FACT  ESM_TAG('F','A','C','T')
#define ESM_RACE  ESM_TAG('R','A','C','E')
#define ESM_CLAS  ESM_TAG('C','L','A','S')
#define ESM_REGN  ESM_TAG('R','E','G','N')
#define ESM_ACTI  ESM_TAG('A','C','T','I')
#define ESM_LEVI  ESM_TAG('L','E','V','I')
#define ESM_LEVC  ESM_TAG('L','E','V','C')

/* Subrecord tags */
#define ESM_NAME  ESM_TAG('N','A','M','E')
#define ESM_DATA  ESM_TAG('D','A','T','A')
#define ESM_FNAM  ESM_TAG('F','N','A','M')
#define ESM_MODL  ESM_TAG('M','O','D','L')
#define ESM_HEDR  ESM_TAG('H','E','D','R')
#define ESM_MAST  ESM_TAG('M','A','S','T')
#define ESM_FRMR  ESM_TAG('F','R','M','R')
#define ESM_WHGT  ESM_TAG('W','H','G','T')
#define ESM_AMBI  ESM_TAG('A','M','B','I')
#define ESM_INTV  ESM_TAG('I','N','T','V')
#define ESM_VNML  ESM_TAG('V','N','M','L')
#define ESM_VHGT  ESM_TAG('V','H','G','T')
#define ESM_WNAM  ESM_TAG('W','N','A','M')
#define ESM_VCLR  ESM_TAG('V','C','L','R')
#define ESM_VTEX  ESM_TAG('V','T','E','X')
#define ESM_NPDT  ESM_TAG('N','P','D','T')
#define ESM_FLAG  ESM_TAG('F','L','A','G')
#define ESM_DODT  ESM_TAG('D','O','D','T')
#define ESM_DNAM  ESM_TAG('D','N','A','M')
#define ESM_XSCL  ESM_TAG('X','S','C','L')
#define ESM_DELE  ESM_TAG('D','E','L','E')
#define ESM_SCRI  ESM_TAG('S','C','R','I')
#define ESM_NAM0  ESM_TAG('N','A','M','0')
#define ESM_NAM5  ESM_TAG('N','A','M','5')
#define ESM_RGNN  ESM_TAG('R','G','N','N')
#define ESM_RNAM  ESM_TAG('R','N','A','M')
#define ESM_CNAM  ESM_TAG('C','N','A','M')
#define ESM_ANAM  ESM_TAG('A','N','A','M')
#define ESM_BNAM  ESM_TAG('B','N','A','M')

/* Cell flags */
#define CELL_INTERIOR   0x01
#define CELL_HAS_WATER  0x02
#define CELL_NO_SLEEP   0x04
#define CELL_QUASI_EX   0x80  /* Interior behaves like exterior */

/* LAND flags */
#define LAND_HAS_HEIGHTS  0x01
#define LAND_HAS_COLORS   0x02
#define LAND_HAS_TEXTURES 0x04

/* -------------------------------------------------------------------
 * Parsed data types
 * ------------------------------------------------------------------- */

/* 3D position (floats in ESM, converted to fixed-point for N64) */
typedef struct {
    float x, y, z;
} esm_vec3f_t;

/* Color (RGBA) */
typedef struct {
    uint8_t r, g, b, a;
} esm_color_t;

/* TES3 header */
typedef struct {
    float    version;
    int32_t  type;          /* 0=ESP, 1=ESM, 32=Save */
    char     author[32];
    char     description[256];
    int32_t  record_count;
} esm_header_t;

/* Cell data */
typedef struct {
    char     name[64];      /* Cell name/ID */
    int32_t  flags;
    int32_t  grid_x, grid_y; /* Exterior grid coordinates */
    float    water_level;
    esm_color_t ambient;
    esm_color_t sunlight;
    esm_color_t fog;
    float    fog_density;
    bool     is_interior;
} esm_cell_t;

/* Object reference (placed in a cell) */
typedef struct {
    uint32_t  ref_num;        /* Reference ID */
    char      object_id[64];  /* ID of the object being placed */
    esm_vec3f_t pos;          /* World position */
    esm_vec3f_t rot;          /* Rotation (radians) */
    float     scale;          /* Scale (0.5-2.0, default 1.0) */

    /* Door data (optional) */
    bool      is_door;
    esm_vec3f_t door_dest_pos;
    esm_vec3f_t door_dest_rot;
    char      door_dest_cell[64];

    /* Lock data */
    int32_t   lock_level;
    char      key_id[64];
    char      trap_id[64];
} esm_cell_ref_t;

/* Terrain data for one exterior cell */
typedef struct {
    int32_t  grid_x, grid_y;
    uint32_t flags;

    /* Heights: 65x65 grid, reconstructed from deltas.
     * Stored as int16_t (height / 8 to save space) */
    int16_t  heights[65 * 65];

    /* Normals: 65x65 x 3 (int8 XYZ per vertex) */
    int8_t   normals[65 * 65 * 3];

    /* Vertex colors: 65x65 x 3 (RGB per vertex) */
    uint8_t  colors[65 * 65 * 3];
    bool     has_colors;

    /* Texture indices: 16x16 grid */
    uint16_t tex_indices[16 * 16];
    bool     has_textures;
} esm_land_t;

/* Static object definition */
typedef struct {
    char name[64];
    char model[128];
} esm_static_t;

/* NPC definition (simplified for N64) */
typedef struct {
    char     name[64];      /* ID */
    char     display_name[64];
    char     race[64];
    char     class_[64];
    char     faction[64];
    char     model[128];
    char     head[64];
    char     hair[64];
    int16_t  level;
    uint8_t  attributes[8]; /* STR,INT,WIL,AGI,SPD,END,PER,LCK */
    int16_t  health;
    int16_t  magicka;
    int16_t  fatigue;
    uint8_t  disposition;
    int32_t  gold;
    uint8_t  flags;         /* 0x01=female, 0x02=essential */
} esm_npc_t;

/* -------------------------------------------------------------------
 * ESM index (built after parsing, used for fast lookup)
 * ------------------------------------------------------------------- */

#define ESM_MAX_CELLS       4096
#define ESM_MAX_LAND        4096
#define ESM_MAX_STATICS     8192
#define ESM_MAX_NPCS        4096
#define ESM_MAX_REFS_PER_CELL 512

typedef struct {
    /* File position index — allows re-reading records on demand */
    struct {
        int32_t grid_x, grid_y;
        uint32_t file_offset;   /* Position of CELL record in ESM */
        uint32_t name_hash;     /* Hash of cell name (for interior lookup) */
        bool     is_interior;
    } cells[ESM_MAX_CELLS];
    int32_t cell_count;

    struct {
        int32_t grid_x, grid_y;
        uint32_t file_offset;   /* Position of LAND record in ESM */
    } lands[ESM_MAX_LAND];
    int32_t land_count;

    /* Object definitions (loaded once, kept in memory) */
    esm_static_t statics[ESM_MAX_STATICS];
    int32_t static_count;

    /* NPCs (loaded once) */
    esm_npc_t npcs[ESM_MAX_NPCS];
    int32_t npc_count;

    /* Header */
    esm_header_t header;
} esm_index_t;

/* -------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------- */

/* Initialize reader from ESM data in memory */
void esm_reader_init(esm_reader_t *r, const uint8_t *data, uint32_t size);

/* Read next record header. Returns false at EOF. */
bool esm_next_record(esm_reader_t *r);

/* Read next subrecord within current record. Returns false at record end. */
bool esm_next_subrecord(esm_reader_t *r);

/* Skip remaining data in current record */
void esm_skip_record(esm_reader_t *r);

/* Skip remaining data in current subrecord */
void esm_skip_subrecord(esm_reader_t *r);

/* Read typed values from current subrecord */
uint8_t  esm_read_u8(esm_reader_t *r);
int8_t   esm_read_s8(esm_reader_t *r);
uint16_t esm_read_u16(esm_reader_t *r);
int16_t  esm_read_s16(esm_reader_t *r);
uint32_t esm_read_u32(esm_reader_t *r);
int32_t  esm_read_s32(esm_reader_t *r);
float    esm_read_f32(esm_reader_t *r);
void     esm_read_string(esm_reader_t *r, char *buf, uint32_t max_len);
void     esm_read_bytes(esm_reader_t *r, void *buf, uint32_t len);

/*
 * Build index: scan entire ESM, record file offsets for all
 * CELL and LAND records. Load STAT/NPC definitions into memory.
 * This is done once at game start (~2 seconds for 79 MB ESM).
 */
void esm_build_index(esm_reader_t *r, esm_index_t *index);

/*
 * Load a specific cell's data (geometry refs, NPCs, etc.)
 * Uses the index to seek directly to the cell's file offset.
 */
bool esm_load_cell(esm_reader_t *r, const esm_index_t *index,
                    int32_t grid_x, int32_t grid_y,
                    esm_cell_t *cell,
                    esm_cell_ref_t *refs, int *ref_count, int max_refs);

/*
 * Load a specific cell's terrain data.
 */
bool esm_load_land(esm_reader_t *r, const esm_index_t *index,
                    int32_t grid_x, int32_t grid_y,
                    esm_land_t *land);

/*
 * Look up an interior cell by name.
 */
bool esm_load_interior(esm_reader_t *r, const esm_index_t *index,
                         const char *name,
                         esm_cell_t *cell,
                         esm_cell_ref_t *refs, int *ref_count, int max_refs);

#endif /* MW_ESM_H */
