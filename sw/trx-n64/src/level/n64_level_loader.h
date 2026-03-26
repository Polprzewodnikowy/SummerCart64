/*
 * TRX-N64: Level loader bridge
 *
 * Replaces TRX's file-based level loading with asset streaming.
 *
 * Architecture:
 *   Original TRX: fopen("level1.phd") → fread chunks sequentially
 *   N64 port:     asset_load(LEVEL_1) → sequential read from cached buffer
 *
 * TR1 .PHD level files are read sequentially with no seeking.
 * This maps perfectly to our asset streaming model:
 *   1. Pin the level asset (keep in cache during gameplay)
 *   2. Read through it sequentially using n64_file_t
 *   3. Texture pages and audio samples are separate assets
 *      (streamed on demand, not all loaded at once)
 *
 * Asset ID layout for a TR1 game:
 *   1-15      Level geometry (.PHD minus texture pages)
 *   100-115   Level texture pages (one asset per page)
 *   200-215   Level audio sample banks
 *   300+      Music tracks
 *   500+      FMV data (optional, low priority)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TRX_N64_LEVEL_LOADER_H
#define TRX_N64_LEVEL_LOADER_H

#include <stdint.h>
#include <stdbool.h>

/* -------------------------------------------------------------------
 * Asset ID scheme
 * ------------------------------------------------------------------- */

#define ASSET_LEVEL_BASE        1       /* Level 1 geometry = asset 1 */
#define ASSET_TEXPAGE_BASE      100     /* Level 1, page 0 = asset 100 */
#define ASSET_TEXPAGE_STRIDE    16      /* 16 pages per level max */
#define ASSET_SAMPLES_BASE      200     /* Level 1 samples = asset 200 */
#define ASSET_MUSIC_BASE        300     /* Music track 0 = asset 300 */

/* Compute asset IDs */
#define LEVEL_ASSET_ID(level_num)       (ASSET_LEVEL_BASE + (level_num))
#define TEXPAGE_ASSET_ID(level, page)   (ASSET_TEXPAGE_BASE + (level) * ASSET_TEXPAGE_STRIDE + (page))
#define SAMPLES_ASSET_ID(level_num)     (ASSET_SAMPLES_BASE + (level_num))
#define MUSIC_ASSET_ID(track_num)       (ASSET_MUSIC_BASE + (track_num))

/* -------------------------------------------------------------------
 * Level state
 * ------------------------------------------------------------------- */

typedef struct {
    /* Level geometry (pinned in cache while playing) */
    uint32_t    level_asset_id;
    void       *level_handle;   /* asset_handle_t from streaming system */

    /* Texture pages loaded on demand */
    uint32_t    tex_page_count;
    bool        tex_page_loaded[16];

    /* Level metadata */
    uint32_t    level_num;
    uint32_t    level_size;

    /* Sequential reader position (for TRX section reader) */
    uint32_t    read_pos;
    const uint8_t *level_data;
} n64_level_state_t;

/* -------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------- */

/*
 * Begin loading a level. Pins the level asset and reads the header.
 * Returns false if the level asset can't be loaded.
 */
bool n64_level_begin(n64_level_state_t *state, uint32_t level_num);

/*
 * Sequential read from level data (replaces fread in TRX loader).
 * This is called by the TRX Level_Section_Read* functions.
 */
int32_t n64_level_read(n64_level_state_t *state, void *buf, uint32_t size);

/*
 * Skip bytes in level data (replaces fseek in TRX loader).
 */
void n64_level_skip(n64_level_state_t *state, uint32_t bytes);

/*
 * Read a specific type from level data.
 * Convenience wrappers for the TRX section readers.
 */
int8_t   n64_level_read_s8(n64_level_state_t *s);
uint8_t  n64_level_read_u8(n64_level_state_t *s);
int16_t  n64_level_read_s16(n64_level_state_t *s);
uint16_t n64_level_read_u16(n64_level_state_t *s);
int32_t  n64_level_read_s32(n64_level_state_t *s);
uint32_t n64_level_read_u32(n64_level_state_t *s);

/*
 * Load texture pages for the current level.
 * Called after level geometry is parsed, before rendering starts.
 * Loads texture pages via asset streaming and converts to N64 format.
 */
bool n64_level_load_textures(n64_level_state_t *state);

/*
 * Unload current level. Unpins the level asset and frees texture pages.
 */
void n64_level_end(n64_level_state_t *state);

#endif /* TRX_N64_LEVEL_LOADER_H */
