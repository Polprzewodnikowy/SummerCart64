/*
 * TRX-N64: Texture management for N64 TMEM
 *
 * TR1 textures: 256×256 8-bit palettized pages with a 256-color palette.
 * Each OBJECT_TEXTURE defines a sub-rectangle via 4 UV coordinates.
 *
 * N64 TMEM: 4096 bytes total.
 *   - CI8 (8-bit color index): max 4096 pixels (64×64)
 *   - CI4 (4-bit color index): max 8192 pixels (128×64 or 64×128)
 *   - RGBA16: max 2048 pixels (64×32)
 *
 * Strategy for 100% faithful textures:
 *   We use CI8 format to preserve the EXACT original palette indices.
 *   No color quantization, no dithering — pixel-perfect reproduction.
 *
 *   For each face:
 *     1. Look up OBJECT_TEXTURE → get tex_page + UV rect
 *     2. Compute bounding box of the 4 UV coords → tile sub-rect
 *     3. Load that sub-rect from the pre-converted page into TMEM
 *     4. Set tile descriptor with correct dimensions and palette
 *     5. Submit textured triangle with remapped UVs
 *
 *   TR1 texture tiles are typically 64×64 or smaller, fitting in TMEM.
 *   Larger tiles (e.g., 128×128) are handled by splitting faces or
 *   using CI4 mode for tiles with ≤16 unique colors.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef N64_TEXTURES_H
#define N64_TEXTURES_H

#include "n64_render.h"
#include "../level/tr_types.h"

#include <stdint.h>
#include <stdbool.h>

/* -------------------------------------------------------------------
 * Pre-converted texture pages
 *
 * On level load, we convert TR1's 8-bit palettized pages into
 * N64-ready CI8 format with RGBA5551 palette.
 *
 * CI8 format: each pixel is an 8-bit index into a 256-color TLUT.
 * This preserves the EXACT original TR1 palette mapping.
 * ------------------------------------------------------------------- */

#define N64_TEX_PAGE_W          256
#define N64_TEX_PAGE_H          256
#define N64_TEX_PAGE_SIZE       (N64_TEX_PAGE_W * N64_TEX_PAGE_H) /* 64 KB */
#define N64_MAX_TEX_PAGES       32
#define N64_PALETTE_ENTRIES     256
#define N64_PALETTE_SIZE        (N64_PALETTE_ENTRIES * 2) /* RGBA5551 */

typedef struct {
    /*
     * CI8 pixel data — IDENTICAL layout to TR1's original 8-bit page.
     * No re-ordering, no quantization. Each byte is the original
     * palette index from the .PHD file.
     */
    uint8_t  pixels[N64_TEX_PAGE_SIZE];

    /* RGBA5551 palette (converted from TR1's RGB888 palette) */
    uint16_t palette[N64_PALETTE_ENTRIES] __attribute__((aligned(8)));

    bool loaded;
} n64_texture_page_t;

/* -------------------------------------------------------------------
 * Texture tile descriptor (what to load into TMEM for a face)
 * ------------------------------------------------------------------- */

typedef struct {
    uint16_t page;          /* Texture page index */
    uint16_t x, y;          /* Top-left corner in page (pixels) */
    uint16_t w, h;          /* Tile dimensions (pixels) */
    uint16_t tmem_words;    /* Size in TMEM (bytes) */
    bool     fits_tmem;     /* Does it fit in 4KB TMEM? */
} tex_tile_desc_t;

/* -------------------------------------------------------------------
 * TMEM state tracking
 * ------------------------------------------------------------------- */

typedef struct {
    /* Currently loaded tile */
    int32_t  current_page;
    int32_t  current_x, current_y;
    int32_t  current_w, current_h;
    bool     palette_loaded;    /* Is the palette for current_page in TMEM? */

    /* Stats */
    uint32_t loads_this_frame;
    uint32_t palette_loads_this_frame;
    uint32_t cache_hits;
} tmem_state_t;

/* -------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------- */

/*
 * Initialize texture system. Convert TR1 palette and pages to N64 format.
 * Preserves every pixel value exactly.
 */
void n64_tex_init(n64_texture_page_t *pages,
                  const uint8_t *tr1_pages,
                  const uint8_t *tr1_palette_rgb,
                  int page_count);

/*
 * Compute the tile descriptor for a face's texture.
 * Extracts the bounding box of the UV coordinates to determine
 * exactly which sub-rectangle of the texture page is needed.
 */
void n64_tex_get_tile(const tr_object_texture_t *tex,
                      int vertex_count,
                      tex_tile_desc_t *out);

/*
 * Load a texture tile into TMEM via RDP commands.
 * Only emits commands if the tile isn't already loaded.
 *
 * After this call, RDP tile 0 is configured for this texture.
 * The caller submits textured triangles referencing tile 0.
 */
void n64_tex_bind(rdp_cmd_buffer_t *buf,
                  tmem_state_t *tmem,
                  const n64_texture_page_t *pages,
                  const tex_tile_desc_t *tile);

/*
 * Convert TR1 UV coordinates (uint16_t, 0-65535) to RDP tile-relative
 * texture coordinates (10.5 fixed-point, relative to the loaded tile).
 *
 * This remaps the UV from page-space to tile-space so the RDP samples
 * correctly from the loaded sub-rectangle.
 */
int16_t n64_tex_remap_u(uint16_t tr1_u, uint16_t tile_x, uint16_t tile_w);
int16_t n64_tex_remap_v(uint16_t tr1_v, uint16_t tile_y, uint16_t tile_h);

/*
 * Reset TMEM tracking state at start of frame.
 */
void n64_tex_frame_reset(tmem_state_t *tmem);

#endif /* N64_TEXTURES_H */
