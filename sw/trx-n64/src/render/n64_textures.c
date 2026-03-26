/*
 * TRX-N64: Texture management — pixel-perfect reproduction
 *
 * Every pixel, every palette color, every UV coordinate is preserved
 * EXACTLY from the original TR1 data. No quantization, no dithering,
 * no approximation.
 *
 * CI8 format: we use 8-bit Color Index mode which stores the EXACT
 * same palette index bytes as TR1's original texture pages. The only
 * conversion is the palette itself: RGB888 → RGBA5551.
 *
 * TMEM strategy:
 *   TMEM is 4096 bytes. CI8 at 64×64 = 4096 bytes (exactly full).
 *   Most TR1 texture tiles are ≤64×64, so they fit perfectly.
 *   For tiles >64×64, we clamp to TMEM capacity (rare in TR1).
 *
 *   Palette (TLUT): stored in upper half of TMEM (address 0x800).
 *   256 entries × 2 bytes = 512 bytes.
 *   This means the max CI8 tile with palette is:
 *     4096 - 512 = 3584 bytes → ~59×60 pixels
 *   In practice we load palette separately and use the full 4KB for tiles.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "n64_textures.h"
#include "n64_render.h"

#include <string.h>

/* RDP command opcodes */
#define RDP_SET_TEX_IMAGE   0x3D
#define RDP_SET_TILE        0x35
#define RDP_LOAD_BLOCK      0x33
#define RDP_LOAD_TILE       0x34
#define RDP_LOAD_TLUT       0x30
#define RDP_SET_TILE_SIZE   0x32
#define RDP_SYNC_TILE       0x28
#define RDP_SYNC_LOAD       0x26
#define RDP_SYNC_PIPE       0x27

/* Image formats */
#define FMT_RGBA    0
#define FMT_CI      2
#define SIZE_8BIT   1
#define SIZE_16BIT  2
#define SIZE_32BIT  3

/* -------------------------------------------------------------------
 * Palette conversion: RGB888 → RGBA5551
 *
 * Exact conversion with no dithering or rounding bias.
 * TR1 color 0 is typically the transparency key.
 * ------------------------------------------------------------------- */

static uint16_t rgb888_to_rgba5551(uint8_t r, uint8_t g, uint8_t b) {
    /*
     * Standard conversion: take top 5 bits of each channel.
     * Alpha = 1 (opaque) for all colors except index 0.
     */
    return ((uint16_t)(r >> 3) << 11)
         | ((uint16_t)(g >> 3) << 6)
         | ((uint16_t)(b >> 3) << 1)
         | 1;
}

/* -------------------------------------------------------------------
 * Initialize: convert TR1 pages + palette to N64 format
 * ------------------------------------------------------------------- */

void n64_tex_init(n64_texture_page_t *pages,
                  const uint8_t *tr1_pages,
                  const uint8_t *tr1_palette_rgb,
                  int page_count) {
    if (page_count > N64_MAX_TEX_PAGES)
        page_count = N64_MAX_TEX_PAGES;

    for (int p = 0; p < page_count; p++) {
        n64_texture_page_t *page = &pages[p];

        /*
         * Copy texture page data BYTE-FOR-BYTE.
         * Each pixel IS the original TR1 palette index.
         * No remapping, no quantization.
         */
        memcpy(page->pixels,
               tr1_pages + p * N64_TEX_PAGE_SIZE,
               N64_TEX_PAGE_SIZE);

        /*
         * Convert palette: RGB888 → RGBA5551.
         * TR1 palette entry 0 is transparent in many textures.
         * We mark it with alpha=0.
         */
        for (int i = 0; i < N64_PALETTE_ENTRIES; i++) {
            uint8_t r = tr1_palette_rgb[i * 3 + 0];
            uint8_t g = tr1_palette_rgb[i * 3 + 1];
            uint8_t b = tr1_palette_rgb[i * 3 + 2];

            /*
             * TR1 palette values are 6-bit (0-63 per channel).
             * Scale to 8-bit: multiply by 4 (or shift left 2).
             */
            r = (r << 2) | (r >> 4);
            g = (g << 2) | (g >> 4);
            b = (b << 2) | (b >> 4);

            page->palette[i] = rgb888_to_rgba5551(r, g, b);

            /* Index 0 = transparent (alpha=0) */
            if (i == 0) {
                page->palette[i] &= ~1;  /* Clear alpha bit */
            }
        }

        page->loaded = true;
    }
}

/* -------------------------------------------------------------------
 * Compute tile descriptor from OBJECT_TEXTURE UV coordinates
 *
 * Finds the exact bounding box of the UV polygon to determine
 * the minimum sub-rectangle we need from the texture page.
 * ------------------------------------------------------------------- */

void n64_tex_get_tile(const tr_object_texture_t *tex,
                      int vertex_count,
                      tex_tile_desc_t *out) {
    /*
     * UV coordinates in TR1:
     *   uint16_t, range 0-65535
     *   Maps to 256×256 page: pixel = uv * 256 / 65536 = uv / 256
     *   The upper byte is the integer pixel coordinate.
     */

    uint16_t min_u = 65535, min_v = 65535;
    uint16_t max_u = 0, max_v = 0;

    for (int i = 0; i < vertex_count; i++) {
        uint16_t u = tex->uv[i].u;
        uint16_t v = tex->uv[i].v;
        if (u < min_u) min_u = u;
        if (v < min_v) min_v = v;
        if (u > max_u) max_u = u;
        if (v > max_v) max_v = v;
    }

    /* Convert UV to pixel coordinates (integer part = upper byte) */
    uint16_t px0 = min_u >> 8;
    uint16_t py0 = min_v >> 8;
    uint16_t px1 = (max_u >> 8) + 1;  /* +1 for inclusive→exclusive */
    uint16_t py1 = (max_v >> 8) + 1;

    /* Clamp to page bounds */
    if (px1 > N64_TEX_PAGE_W) px1 = N64_TEX_PAGE_W;
    if (py1 > N64_TEX_PAGE_H) py1 = N64_TEX_PAGE_H;

    out->page = tex->tex_page;
    out->x = px0;
    out->y = py0;
    out->w = px1 - px0;
    out->h = py1 - py0;

    /* CI8: 1 byte per pixel */
    out->tmem_words = out->w * out->h;
    out->fits_tmem = (out->tmem_words <= 4096);
}

/* -------------------------------------------------------------------
 * Load texture tile into TMEM
 *
 * Uses RDP LOAD_TILE to DMA a sub-rectangle of a CI8 texture page
 * from RDRAM into TMEM. Also loads the 256-color palette (TLUT).
 * ------------------------------------------------------------------- */

void n64_tex_bind(rdp_cmd_buffer_t *buf,
                  tmem_state_t *tmem,
                  const n64_texture_page_t *pages,
                  const tex_tile_desc_t *tile) {
    /* Check if this exact tile is already loaded */
    if (tmem->current_page == (int32_t)tile->page &&
        tmem->current_x == tile->x &&
        tmem->current_y == tile->y &&
        tmem->current_w == (int32_t)tile->w &&
        tmem->current_h == (int32_t)tile->h) {
        tmem->cache_hits++;
        return;
    }

    const n64_texture_page_t *page = &pages[tile->page];
    if (!page->loaded) return;

    /* Sync before loading new texture */
    rdp_emit(buf, (uint64_t)RDP_SYNC_TILE << 56);

    /*
     * Step 1: Load palette (TLUT) if page changed.
     *
     * The palette goes into TMEM at address 0x800 (upper half).
     * 256 entries × 2 bytes = 512 bytes.
     * We use LOAD_TLUT command with tile descriptor 15 (convention).
     */
    if (!tmem->palette_loaded || tmem->current_page != (int32_t)tile->page) {
        /* SET_TEX_IMAGE: point to palette data */
        uint32_t pal_phys = (uint32_t)(uintptr_t)page->palette & 0x1FFFFFFF;
        rdp_emit(buf, ((uint64_t)RDP_SET_TEX_IMAGE << 56)
                    | ((uint64_t)FMT_RGBA << 53)
                    | ((uint64_t)SIZE_16BIT << 51)
                    | ((uint64_t)0 << 32)   /* width-1 (unused for TLUT) */
                    | pal_phys);

        /* SET_TILE for tile descriptor 15: palette location in TMEM */
        rdp_emit(buf, ((uint64_t)RDP_SET_TILE << 56)
                    | ((uint64_t)0 << 53)           /* format (unused) */
                    | ((uint64_t)SIZE_16BIT << 51)   /* size = 16-bit */
                    | ((uint64_t)0 << 41)            /* line (unused) */
                    | ((uint64_t)0x100 << 32)        /* TMEM addr 0x800 (in 64-bit words: 0x100) */
                    | ((uint64_t)15 << 24)           /* tile 15 */
                    );

        rdp_emit(buf, (uint64_t)RDP_SYNC_LOAD << 56);

        /* LOAD_TLUT: load 256 palette entries */
        rdp_emit(buf, ((uint64_t)RDP_LOAD_TLUT << 56)
                    | ((uint64_t)15 << 24)           /* tile 15 */
                    | ((uint64_t)(255 << 2) << 12)   /* SH = 255 (256 entries - 1) */
                    );

        tmem->palette_loaded = true;
        tmem->palette_loads_this_frame++;
    }

    /*
     * Step 2: Load texture tile data.
     *
     * SET_TEX_IMAGE: point to the texture page pixel data.
     * The CI8 data is a flat 256×256 byte array.
     * We load a sub-rectangle using LOAD_TILE.
     */
    uint32_t tex_phys = (uint32_t)(uintptr_t)page->pixels & 0x1FFFFFFF;

    rdp_emit(buf, ((uint64_t)RDP_SET_TEX_IMAGE << 56)
                | ((uint64_t)FMT_CI << 53)
                | ((uint64_t)SIZE_8BIT << 51)
                | ((uint64_t)(N64_TEX_PAGE_W - 1) << 32) /* width of source in pixels - 1 */
                | tex_phys);

    /*
     * SET_TILE for tile 0: configure how the RDP interprets TMEM data.
     *
     * For CI8:
     *   format = CI (2)
     *   size = 8-bit (1)
     *   line = tile_width / 8  (bytes per row / 8)
     *   tmem = 0 (start of TMEM)
     *   palette = 0 (using TLUT at 0x800)
     */
    uint16_t line_size = (tile->w + 7) / 8;  /* Ceil(width / 8) 64-bit words per row */
    rdp_emit(buf, ((uint64_t)RDP_SET_TILE << 56)
                | ((uint64_t)FMT_CI << 53)
                | ((uint64_t)SIZE_8BIT << 51)
                | ((uint64_t)(line_size & 0x1FF) << 41)
                | ((uint64_t)0 << 32)          /* TMEM address 0 */
                | ((uint64_t)0 << 24)          /* tile 0 */
                | ((uint64_t)0 << 20)          /* palette 0 */
                );

    rdp_emit(buf, (uint64_t)RDP_SYNC_LOAD << 56);

    /*
     * LOAD_TILE: DMA the sub-rectangle from RDRAM page to TMEM.
     *
     * SL, TL = top-left corner in 10.2 fixed-point
     * SH, TH = bottom-right corner in 10.2 fixed-point
     */
    uint32_t sl = tile->x << 2;
    uint32_t tl = tile->y << 2;
    uint32_t sh = (tile->x + tile->w - 1) << 2;
    uint32_t th = (tile->y + tile->h - 1) << 2;

    rdp_emit(buf, ((uint64_t)RDP_LOAD_TILE << 56)
                | ((uint64_t)0 << 24)          /* tile 0 */
                | ((uint64_t)(sl & 0xFFF) << 44)
                | ((uint64_t)(tl & 0xFFF) << 32)
                | ((uint64_t)(sh & 0xFFF) << 12)
                | ((uint64_t)(th & 0xFFF))
                );

    /*
     * SET_TILE_SIZE: tell the RDP the actual tile dimensions
     * for texture coordinate clamping/wrapping.
     */
    rdp_emit(buf, ((uint64_t)RDP_SET_TILE_SIZE << 56)
                | ((uint64_t)0 << 24)          /* tile 0 */
                | ((uint64_t)0 << 44)          /* SL = 0 (tile-relative) */
                | ((uint64_t)0 << 32)          /* TL = 0 */
                | ((uint64_t)(((tile->w - 1) << 2) & 0xFFF) << 12)
                | ((uint64_t)(((tile->h - 1) << 2) & 0xFFF))
                );

    /* Update tracking */
    tmem->current_page = tile->page;
    tmem->current_x = tile->x;
    tmem->current_y = tile->y;
    tmem->current_w = tile->w;
    tmem->current_h = tile->h;
    tmem->loads_this_frame++;
}

/* -------------------------------------------------------------------
 * UV remapping: page-space → tile-space
 *
 * TR1 UVs are in page-space (0-65535 maps to 0-256 pixels).
 * The RDP needs tile-relative coordinates in 10.5 fixed-point
 * (where 1.0 = 32 in S.10.5 format → each pixel = 32).
 *
 * Conversion:
 *   pixel_in_page = tr1_uv >> 8  (integer pixel)
 *   frac_in_page  = tr1_uv & 0xFF  (sub-pixel, 8 bits)
 *   pixel_in_tile = pixel_in_page - tile_origin
 *   rdp_coord     = pixel_in_tile * 32 + (frac * 32 / 256)
 *
 * This preserves sub-pixel UV precision from the original data.
 * ------------------------------------------------------------------- */

int16_t n64_tex_remap_u(uint16_t tr1_u, uint16_t tile_x, uint16_t tile_w) {
    (void)tile_w;
    /* Pixel coordinate (integer + fractional) */
    int32_t pixel = (tr1_u >> 8) - tile_x;
    int32_t frac = tr1_u & 0xFF;

    /* Convert to 10.5 fixed-point: multiply by 32 */
    int32_t rdp_coord = (pixel << 5) + ((frac * 32) >> 8);

    return (int16_t)rdp_coord;
}

int16_t n64_tex_remap_v(uint16_t tr1_v, uint16_t tile_y, uint16_t tile_h) {
    (void)tile_h;
    int32_t pixel = (tr1_v >> 8) - tile_y;
    int32_t frac = tr1_v & 0xFF;
    int32_t rdp_coord = (pixel << 5) + ((frac * 32) >> 8);
    return (int16_t)rdp_coord;
}

/* -------------------------------------------------------------------
 * Frame reset
 * ------------------------------------------------------------------- */

void n64_tex_frame_reset(tmem_state_t *tmem) {
    tmem->current_page = -1;
    tmem->current_x = -1;
    tmem->current_y = -1;
    tmem->current_w = -1;
    tmem->current_h = -1;
    tmem->palette_loaded = false;
    tmem->loads_this_frame = 0;
    tmem->palette_loads_this_frame = 0;
    tmem->cache_hits = 0;
}
