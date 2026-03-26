/*
 * TRX-N64: N64 RDP rendering backend
 *
 * Implements the core rendering pipeline for N64 hardware:
 *   - Double-buffered 320×240 16-bit framebuffer
 *   - RDP command buffer construction and submission
 *   - TMEM texture tile cache (4-bit CI format for TR1 textures)
 *   - Vertex transform and triangle submission
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "n64_render.h"
#include <string.h>

/* -------------------------------------------------------------------
 * RDP command helpers (build 64-bit command words)
 * ------------------------------------------------------------------- */

/* RDP command opcodes */
#define RDP_SYNC_FULL       0x29
#define RDP_SYNC_PIPE       0x27
#define RDP_SYNC_TILE       0x28
#define RDP_SYNC_LOAD       0x26
#define RDP_SET_COLOR_IMAGE 0x3F
#define RDP_SET_Z_IMAGE     0x3E
#define RDP_SET_TEX_IMAGE   0x3D
#define RDP_SET_COMBINE     0x3C
#define RDP_SET_TILE        0x35
#define RDP_LOAD_TILE       0x34
#define RDP_LOAD_TLUT       0x30
#define RDP_SET_TILE_SIZE   0x32
#define RDP_SET_OTHER_MODES 0x2F
#define RDP_SET_SCISSOR     0x2D
#define RDP_FILL_RECT       0x36
#define RDP_SET_FILL_COLOR  0x37
#define RDP_SET_PRIM_COLOR  0x3A
#define RDP_TRI_SHADE_TEX_Z 0x0E

/* Image formats */
#define FMT_RGBA    0
#define FMT_CI      2
#define SIZE_4BIT   0
#define SIZE_16BIT  2

static void rdp_emit(rdp_cmd_buffer_t *buf, uint64_t cmd) {
    if (buf->cmd_count < buf->cmd_capacity) {
        buf->cmds[buf->cmd_count++] = cmd;
    }
}

static void rdp_sync_pipe(rdp_cmd_buffer_t *buf) {
    rdp_emit(buf, (uint64_t)RDP_SYNC_PIPE << 56);
}

static void rdp_sync_tile(rdp_cmd_buffer_t *buf) {
    rdp_emit(buf, (uint64_t)RDP_SYNC_TILE << 56);
}

static void rdp_sync_full(rdp_cmd_buffer_t *buf) {
    rdp_emit(buf, (uint64_t)RDP_SYNC_FULL << 56);
}

/* Build SET_COLOR_IMAGE command */
static void rdp_set_color_image(rdp_cmd_buffer_t *buf,
                                 uint8_t fmt, uint8_t size,
                                 uint16_t width, uint32_t addr) {
    uint64_t w0 = ((uint64_t)RDP_SET_COLOR_IMAGE << 56)
                | ((uint64_t)fmt << 53)
                | ((uint64_t)size << 51)
                | ((uint64_t)(width - 1) << 32);
    rdp_emit(buf, w0 | addr);
}

/* Build SET_SCISSOR command */
static void rdp_set_scissor(rdp_cmd_buffer_t *buf,
                             uint16_t x0, uint16_t y0,
                             uint16_t x1, uint16_t y1) {
    uint64_t w = ((uint64_t)RDP_SET_SCISSOR << 56)
               | ((uint64_t)(x0 << 2) << 44)
               | ((uint64_t)(y0 << 2) << 32)
               | ((uint64_t)(x1 << 2) << 12)
               | ((uint64_t)(y1 << 2));
    rdp_emit(buf, w);
}

/* Build FILL_RECT command */
static void rdp_fill_rect(rdp_cmd_buffer_t *buf,
                           uint16_t x0, uint16_t y0,
                           uint16_t x1, uint16_t y1) {
    uint64_t w = ((uint64_t)RDP_FILL_RECT << 56)
               | ((uint64_t)(x1 << 2) << 44)
               | ((uint64_t)(y1 << 2) << 32)
               | ((uint64_t)(x0 << 2) << 12)
               | ((uint64_t)(y0 << 2));
    rdp_emit(buf, w);
}

/* Build SET_FILL_COLOR command */
static void rdp_set_fill_color(rdp_cmd_buffer_t *buf, uint32_t color) {
    rdp_emit(buf, ((uint64_t)RDP_SET_FILL_COLOR << 56) | color);
}

/* Build SET_OTHER_MODES for fill mode */
static void rdp_set_other_modes_fill(rdp_cmd_buffer_t *buf) {
    /* Cycle type = fill (11 << 52) */
    uint64_t w = ((uint64_t)RDP_SET_OTHER_MODES << 56)
               | ((uint64_t)3 << 52);  /* cycle_type = fill */
    rdp_emit(buf, w);
}

/* Build SET_OTHER_MODES for 1-cycle textured rendering */
static void rdp_set_other_modes_1cycle(rdp_cmd_buffer_t *buf, bool z_compare, bool z_update) {
    uint64_t w = ((uint64_t)RDP_SET_OTHER_MODES << 56)
               | ((uint64_t)0 << 52);  /* cycle_type = 1-cycle */
    if (z_compare) w |= (1ULL << 4);   /* z_compare_en */
    if (z_update)  w |= (1ULL << 5);   /* z_update_en */
    w |= (1ULL << 13);  /* image_read_en (for blending) */
    rdp_emit(buf, w);
}

/* -------------------------------------------------------------------
 * Texture page conversion: TR1 8-bit palettized → N64 4-bit CI
 *
 * TR1 texture pages are 256×256 pixels, 8-bit indexed into a
 * 256-color palette (RGB888). We:
 *   1. Convert palette to RGBA5551
 *   2. Split each page into 4×4 grid of 64×64 tiles
 *   3. Reduce each tile to 4-bit (16 colors) using most-used-colors
 *   4. Store in TMEM-ready format
 * ------------------------------------------------------------------- */

/* Convert RGB888 to RGBA5551 */
static uint16_t rgb888_to_rgba5551(uint8_t r, uint8_t g, uint8_t b) {
    return ((r >> 3) << 11) | ((g >> 3) << 6) | ((b >> 3) << 1) | 1;
}

void n64_render_load_tex_pages(n64_renderer_t *r,
                                const uint8_t *pages_8bit,
                                const uint8_t *palette_rgb,
                                uint32_t page_count) {
    if (page_count > MAX_TEX_PAGES) page_count = MAX_TEX_PAGES;
    r->tex_page_count = page_count;

    for (uint32_t p = 0; p < page_count; p++) {
        n64_tex_page_t *page = &r->tex_pages[p];
        const uint8_t *src = pages_8bit + p * TR_TEX_PAGE_W * TR_TEX_PAGE_H;

        /* Convert full palette */
        for (int i = 0; i < 256; i++) {
            page->full_palette[i] = rgb888_to_rgba5551(
                palette_rgb[i * 3 + 0],
                palette_rgb[i * 3 + 1],
                palette_rgb[i * 3 + 2]);
        }

        /* Split into 64×64 tiles and quantize to 4-bit */
        for (int ty = 0; ty < 4; ty++) {
            for (int tx = 0; tx < 4; tx++) {
                int tile_idx = ty * 4 + tx;
                n64_tile_t *tile = &page->tiles[tile_idx];

                /* Count color usage in this tile */
                uint32_t color_count[256] = {0};
                for (int y = 0; y < TR_TILE_H; y++) {
                    for (int x = 0; x < TR_TILE_W; x++) {
                        int px = tx * TR_TILE_W + x;
                        int py = ty * TR_TILE_H + y;
                        uint8_t ci = src[py * TR_TEX_PAGE_W + px];
                        color_count[ci]++;
                    }
                }

                /* Find top 16 colors */
                uint8_t top16[16];
                uint8_t remap[256];
                memset(remap, 0, sizeof(remap));

                for (int c = 0; c < 16; c++) {
                    uint32_t best = 0;
                    int best_idx = 0;
                    for (int i = 0; i < 256; i++) {
                        if (color_count[i] > best) {
                            best = color_count[i];
                            best_idx = i;
                        }
                    }
                    top16[c] = best_idx;
                    remap[best_idx] = c;
                    color_count[best_idx] = 0;
                }

                /* Build palette */
                for (int c = 0; c < 16; c++) {
                    tile->palette[c] = page->full_palette[top16[c]];
                }

                /* Build 4-bit pixel data */
                for (int y = 0; y < TR_TILE_H; y++) {
                    for (int x = 0; x < TR_TILE_W; x += 2) {
                        int px = tx * TR_TILE_W + x;
                        int py = ty * TR_TILE_H + y;
                        uint8_t ci0 = src[py * TR_TEX_PAGE_W + px];
                        uint8_t ci1 = src[py * TR_TEX_PAGE_W + px + 1];

                        /* Map to nearest of top-16 (for non-top colors,
                           remap[ci] defaults to 0 — acceptable for TR1) */
                        uint8_t lo = remap[ci0];
                        uint8_t hi = remap[ci1];

                        int byte_idx = (y * TR_TILE_W + x) / 2;
                        tile->ci4_data[byte_idx] = (lo << 4) | hi;
                    }
                }

                tile->valid = true;
            }
        }

        page->loaded = true;
    }
}

/* -------------------------------------------------------------------
 * TMEM tile cache
 * ------------------------------------------------------------------- */

void n64_render_bind_texture(n64_renderer_t *r, uint16_t tex_page, uint16_t tile_idx) {
    tcache_t *tc = &r->tcache;

    /* Check if already loaded */
    for (uint32_t i = 0; i < tc->entry_count; i++) {
        if (tc->entries[i].tex_page == tex_page &&
            tc->entries[i].tile_index == tile_idx &&
            tc->entries[i].loaded) {
            tc->hits_this_frame++;
            return;
        }
    }

    /* Load tile into TMEM via RDP commands */
    if (tex_page >= r->tex_page_count) return;
    n64_tex_page_t *page = &r->tex_pages[tex_page];
    if (tile_idx >= TR_TILES_PER_PAGE) return;
    n64_tile_t *tile = &page->tiles[tile_idx];
    if (!tile->valid) return;

    rdp_cmd_buffer_t *buf = &r->cmd_buf;

    rdp_sync_tile(buf);

    /*
     * Load 4-bit CI tile into TMEM:
     *   1. SET_TEX_IMAGE → point to tile CI4 data in RDRAM
     *   2. SET_TILE → configure tile descriptor
     *   3. LOAD_TILE → DMA tile data into TMEM
     *   4. LOAD_TLUT → DMA palette into TMEM palette area
     *
     * CI4 64×64 = 2048 bytes → fits in half of TMEM
     * Palette = 16 × 2 = 32 bytes → TMEM address 0x800+
     */

    /* SET_TEX_IMAGE: point to CI4 data */
    uint32_t tile_phys = (uint32_t)(uintptr_t)tile->ci4_data & 0x1FFFFFFF;
    rdp_emit(buf, ((uint64_t)RDP_SET_TEX_IMAGE << 56)
                | ((uint64_t)FMT_CI << 53)
                | ((uint64_t)SIZE_4BIT << 51)
                | ((uint64_t)(TR_TILE_W / 2 - 1) << 32)  /* width in 8-byte units */
                | tile_phys);

    /* SET_TILE: configure tile 0 as CI4, 64 wide */
    rdp_emit(buf, ((uint64_t)RDP_SET_TILE << 56)
                | ((uint64_t)FMT_CI << 53)
                | ((uint64_t)SIZE_4BIT << 51)
                | ((uint64_t)8 << 41)           /* line size (64/8=8 qwords) */
                | ((uint64_t)0 << 32)           /* tmem address 0 */
                | ((uint64_t)0 << 24)           /* tile 0 */
                | ((uint64_t)4 << 8)            /* palette 0 */
                );

    /* LOAD_TILE */
    rdp_emit(buf, ((uint64_t)RDP_LOAD_TILE << 56)
                | ((uint64_t)0 << 44)           /* SL */
                | ((uint64_t)0 << 32)           /* TL */
                | ((uint64_t)((TR_TILE_W - 1) << 2) << 12)  /* SH */
                | ((uint64_t)((TR_TILE_H - 1) << 2))        /* TH */
                );

    /* Now load palette via LOAD_TLUT */
    uint32_t pal_phys = (uint32_t)(uintptr_t)tile->palette & 0x1FFFFFFF;
    rdp_emit(buf, ((uint64_t)RDP_SET_TEX_IMAGE << 56)
                | ((uint64_t)FMT_RGBA << 53)
                | ((uint64_t)SIZE_16BIT << 51)
                | pal_phys);

    rdp_emit(buf, ((uint64_t)RDP_LOAD_TLUT << 56)
                | ((uint64_t)0 << 44)
                | ((uint64_t)0 << 32)
                | ((uint64_t)(15 << 2) << 12)   /* 16 entries */
                | 0);

    /* Track in cache */
    if (tc->entry_count < TCACHE_MAX_TILES) {
        tcache_entry_t *e = &tc->entries[tc->entry_count++];
        e->tex_page = tex_page;
        e->tile_index = tile_idx;
        e->tmem_slot = 0;
        e->format = 0;  /* CI4 */
        e->loaded = true;
    }
    tc->loads_this_frame++;
}

/* -------------------------------------------------------------------
 * Frame lifecycle
 * ------------------------------------------------------------------- */

void n64_render_init(n64_renderer_t *r) {
    memset(r, 0, sizeof(*r));

    /* Allocate framebuffers from a known RDRAM region.
     * In the real build these come from the linker script. */
    extern uint16_t __framebuffer_0[];
    extern uint16_t __framebuffer_1[];
    extern uint16_t __zbuffer[];

    r->fb[0] = __framebuffer_0;
    r->fb[1] = __framebuffer_1;
    r->zbuf = __zbuffer;
    r->current_fb = 0;

    /* Allocate command buffer */
    static uint64_t cmd_storage[RDP_CMD_BUFFER_SIZE / sizeof(uint64_t)]
        __attribute__((aligned(16)));
    r->cmd_buf.cmds = cmd_storage;
    r->cmd_buf.cmd_capacity = RDP_CMD_BUFFER_SIZE / sizeof(uint64_t);

    /* Allocate texture page storage */
    static n64_tex_page_t tex_page_storage[MAX_TEX_PAGES];
    r->tex_pages = tex_page_storage;
}

void n64_render_shutdown(n64_renderer_t *r) {
    (void)r;
}

void n64_render_frame_begin(n64_renderer_t *r) {
    rdp_cmd_buffer_t *buf = &r->cmd_buf;
    buf->cmd_count = 0;

    /* Reset per-frame stats */
    r->tris_drawn = 0;
    r->tmem_loads = 0;
    r->draw_calls = 0;
    r->tcache.entry_count = 0;
    r->tcache.loads_this_frame = 0;
    r->tcache.hits_this_frame = 0;

    /* Set color image to current framebuffer */
    uint32_t fb_phys = (uint32_t)(uintptr_t)r->fb[r->current_fb] & 0x1FFFFFFF;
    rdp_set_color_image(buf, FMT_RGBA, SIZE_16BIT, N64_SCREEN_W, fb_phys);

    /* Set Z image */
    uint32_t zb_phys = (uint32_t)(uintptr_t)r->zbuf & 0x1FFFFFFF;
    rdp_emit(buf, ((uint64_t)RDP_SET_Z_IMAGE << 56) | zb_phys);

    /* Scissor to full screen */
    rdp_set_scissor(buf, 0, 0, N64_SCREEN_W, N64_SCREEN_H);

    /* Clear framebuffer (fill mode) */
    rdp_set_other_modes_fill(buf);
    rdp_set_fill_color(buf, 0x00010001);  /* Black, Z=1 */
    rdp_fill_rect(buf, 0, 0, N64_SCREEN_W, N64_SCREEN_H);

    /* Clear Z buffer */
    rdp_emit(buf, ((uint64_t)RDP_SET_COLOR_IMAGE << 56)
                | ((uint64_t)FMT_RGBA << 53)
                | ((uint64_t)SIZE_16BIT << 51)
                | ((uint64_t)(N64_SCREEN_W - 1) << 32)
                | zb_phys);
    rdp_set_fill_color(buf, 0xFFFFFFFF);  /* Max depth */
    rdp_fill_rect(buf, 0, 0, N64_SCREEN_W, N64_SCREEN_H);

    /* Restore color image */
    rdp_set_color_image(buf, FMT_RGBA, SIZE_16BIT, N64_SCREEN_W, fb_phys);

    /* Switch to 1-cycle textured mode */
    rdp_set_other_modes_1cycle(buf, true, true);

    rdp_sync_pipe(buf);
}

void n64_render_frame_end(n64_renderer_t *r) {
    rdp_cmd_buffer_t *buf = &r->cmd_buf;

    /* Final sync */
    rdp_sync_full(buf);

    /* Submit command buffer to RDP */
    uint32_t cmd_phys = (uint32_t)(uintptr_t)buf->cmds & 0x1FFFFFFF;
    DPC_STATUS = 0x01;  /* Clear XBUS/FREEZE/FLUSH */
    DPC_START = cmd_phys;
    DPC_END = cmd_phys + buf->cmd_count * sizeof(uint64_t);

    /* Wait for RDP to finish */
    while (DPC_STATUS & 0x600) {
        /* Busy-wait for pipe busy + command busy to clear */
    }

    /* Swap framebuffers */
    VI_ORIGIN = (uint32_t)(uintptr_t)r->fb[r->current_fb] & 0x1FFFFFFF;
    r->current_fb ^= 1;

    /* Update perf counters */
    r->tmem_loads = r->tcache.loads_this_frame;
}

/* -------------------------------------------------------------------
 * Triangle submission
 *
 * The N64 RDP triangle command is complex (36+ bytes).
 * For now we build the edge coefficients in software.
 * Future optimization: use RSP microcode for vertex transform.
 * ------------------------------------------------------------------- */

void n64_render_tri(n64_renderer_t *r, const n64_vertex_t v[3]) {
    /*
     * TODO: Build RDP_TRI_SHADE_TEX_Z command from vertices.
     *
     * This requires computing edge slopes and coefficient deltas.
     * The full command is 176 bits (22 bytes) minimum.
     * For the initial port, we'll start with flat-shaded triangles
     * and add texture mapping once the geometry pipeline works.
     *
     * For now: increment counter, actual command generation follows.
     */
    r->tris_drawn++;
    r->draw_calls++;

    (void)v;  /* Will be used when edge coefficient math is implemented */
}

void n64_render_quad(n64_renderer_t *r, const n64_vertex_t v[4]) {
    /* Split quad into two triangles */
    n64_vertex_t tri1[3] = {v[0], v[1], v[2]};
    n64_vertex_t tri2[3] = {v[0], v[2], v[3]};
    n64_render_tri(r, tri1);
    n64_render_tri(r, tri2);
}

/* -------------------------------------------------------------------
 * TRX Output_Draw* wrappers
 *
 * These will be filled in as we wire up the TRX game logic.
 * Each one transforms TRX data structures → N64 vertices → RDP.
 * ------------------------------------------------------------------- */

void n64_draw_room(n64_renderer_t *r, const void *room, bool is_outside) {
    (void)r; (void)room; (void)is_outside;
    /* TODO: iterate room mesh faces, transform vertices, submit tris */
}

void n64_draw_object_mesh(n64_renderer_t *r, const void *mesh, int clip) {
    (void)r; (void)mesh; (void)clip;
    /* TODO: iterate mesh faces, transform vertices, submit tris */
}

void n64_draw_sprite(n64_renderer_t *r,
                     int32_t x, int32_t y, int32_t z,
                     int16_t sprnum, int16_t shade) {
    (void)r; (void)x; (void)y; (void)z; (void)sprnum; (void)shade;
    /* TODO: billboard sprite rendering */
}

void n64_draw_shadow(n64_renderer_t *r, int16_t size,
                     const void *bounds, const void *item) {
    (void)r; (void)size; (void)bounds; (void)item;
    /* TODO: shadow blob rendering */
}

void n64_draw_screen_flat_quad(n64_renderer_t *r,
                                int32_t sx, int32_t sy, int32_t w, int32_t h,
                                uint32_t color_rgba) {
    rdp_cmd_buffer_t *buf = &r->cmd_buf;
    rdp_sync_pipe(buf);
    rdp_set_other_modes_fill(buf);
    /* Pack RGBA8888 into fill color (two RGBA5551 pixels) */
    uint16_t c16 = ((color_rgba >> 27) << 11)
                  | (((color_rgba >> 19) & 0x1F) << 6)
                  | (((color_rgba >> 11) & 0x1F) << 1)
                  | 1;
    rdp_set_fill_color(buf, ((uint32_t)c16 << 16) | c16);
    rdp_fill_rect(buf, sx, sy, sx + w, sy + h);
    r->draw_calls++;
}

void n64_draw_screen_sprite(n64_renderer_t *r,
                             int32_t sx, int32_t sy,
                             int32_t scale_h, int32_t scale_v,
                             int32_t sprite_idx) {
    (void)r; (void)sx; (void)sy; (void)scale_h; (void)scale_v;
    (void)sprite_idx;
    /* TODO: textured screen-space sprite */
}

/* -------------------------------------------------------------------
 * Debug overlay
 * ------------------------------------------------------------------- */

void n64_render_debug_overlay(n64_renderer_t *r) {
    /* Draw a small stats bar at top of screen */
    n64_draw_screen_flat_quad(r, 0, 0, N64_SCREEN_W, 10, 0x00000080);

    /*
     * TODO: draw text overlay with:
     *   - FPS
     *   - Triangle count
     *   - TMEM loads vs hits
     *   - Cache slot usage
     *
     * Will use the bootloader's font rendering adapted for
     * direct framebuffer writes.
     */
}
