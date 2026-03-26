/*
 * TRX-N64: Tomb Raider 1 — N64 + SC64 Port
 *
 * N64 RDP rendering backend.
 *
 * Replaces TRX's OpenGL renderer (src/trx/gl/) with N64 Reality
 * Display Processor commands. Implements the Output_Draw* interface
 * that the game logic calls.
 *
 * N64 RDP constraints:
 *   - 4 KB TMEM for textures (vs unlimited GPU VRAM)
 *   - 320×240 framebuffer (vs arbitrary resolution)
 *   - ~100K triangles/frame budget
 *   - No shaders — fixed-function pipeline with combiner modes
 *
 * Strategy:
 *   - TR1 textures are 256-color palettized, 64×64 tiles
 *   - At 4-bit CI format: 64×64 = 2 KB + 32B palette = fits in TMEM
 *   - At 8-bit CI format: 64×64 = 4 KB — uses full TMEM per tile
 *   - We use 4-bit CI where possible (16 colors per tile)
 *   - Texture page cache: convert on level load, stream tiles to TMEM
 *   - Room faces are already sorted by texture in TRX — batch by tile
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TRX_N64_RENDER_H
#define TRX_N64_RENDER_H

#include <stdint.h>
#include <stdbool.h>

/* -------------------------------------------------------------------
 * N64 Video Interface / RDP register addresses
 * ------------------------------------------------------------------- */

#ifndef VI_BASE
#define VI_BASE             0xA4400000
#define VI_STATUS           (*(volatile uint32_t *)(VI_BASE + 0x00))
#define VI_ORIGIN           (*(volatile uint32_t *)(VI_BASE + 0x04))
#define VI_WIDTH            (*(volatile uint32_t *)(VI_BASE + 0x08))
#define VI_V_INTR           (*(volatile uint32_t *)(VI_BASE + 0x0C))
#define VI_V_CURRENT        (*(volatile uint32_t *)(VI_BASE + 0x10))
#define VI_BURST            (*(volatile uint32_t *)(VI_BASE + 0x14))
#define VI_V_SYNC           (*(volatile uint32_t *)(VI_BASE + 0x18))
#define VI_H_SYNC           (*(volatile uint32_t *)(VI_BASE + 0x1C))
#define VI_H_SYNC_LEAP      (*(volatile uint32_t *)(VI_BASE + 0x20))
#define VI_H_VIDEO           (*(volatile uint32_t *)(VI_BASE + 0x24))
#define VI_V_VIDEO           (*(volatile uint32_t *)(VI_BASE + 0x28))
#define VI_V_BURST           (*(volatile uint32_t *)(VI_BASE + 0x2C))
#define VI_X_SCALE           (*(volatile uint32_t *)(VI_BASE + 0x30))
#define VI_Y_SCALE           (*(volatile uint32_t *)(VI_BASE + 0x34))

#endif /* VI_BASE */

#ifndef DPC_BASE
#define DPC_BASE            0xA4100000
#define DPC_START           (*(volatile uint32_t *)(DPC_BASE + 0x00))
#define DPC_END             (*(volatile uint32_t *)(DPC_BASE + 0x04))
#define DPC_STATUS          (*(volatile uint32_t *)(DPC_BASE + 0x0C))
#endif /* DPC_BASE */

/* -------------------------------------------------------------------
 * Display configuration
 * ------------------------------------------------------------------- */

#define N64_SCREEN_W        320
#define N64_SCREEN_H        240
#define N64_FB_SIZE         (N64_SCREEN_W * N64_SCREEN_H * 2) /* 16-bit color */
#define N64_ZBUF_SIZE       (N64_SCREEN_W * N64_SCREEN_H * 2)

/* TMEM layout for TR1 textures */
#define TMEM_SIZE           4096
#define TR_TILE_W           64
#define TR_TILE_H           64
#define TR_TILE_4BIT_SIZE   (TR_TILE_W * TR_TILE_H / 2)  /* 2048 bytes */
#define TR_PALETTE_SIZE     32                             /* 16 colors × 2 bytes */

/* -------------------------------------------------------------------
 * RDP command buffer
 * ------------------------------------------------------------------- */

#define RDP_CMD_BUFFER_SIZE (32 * 1024)  /* 32 KB command buffer */

typedef struct {
    uint64_t *cmds;         /* Command buffer (64-bit aligned) */
    uint32_t cmd_count;     /* Number of 64-bit words written */
    uint32_t cmd_capacity;  /* Max 64-bit words */
} rdp_cmd_buffer_t;

static inline void rdp_emit(rdp_cmd_buffer_t *buf, uint64_t cmd) {
    if (buf->cmd_count < buf->cmd_capacity) {
        buf->cmds[buf->cmd_count++] = cmd;
    }
}

/* -------------------------------------------------------------------
 * Texture cache (TMEM management)
 * ------------------------------------------------------------------- */

#define TCACHE_MAX_TILES    256  /* Max unique tiles loaded per frame */

typedef struct {
    uint16_t tex_page;      /* Source texture page */
    uint16_t tile_index;    /* Tile within page */
    uint8_t  tmem_slot;     /* Which TMEM slot (0 or 1 for 4-bit) */
    uint8_t  format;        /* 0=CI4, 1=CI8, 2=RGBA16 */
    bool     loaded;        /* Currently in TMEM? */
} tcache_entry_t;

typedef struct {
    tcache_entry_t entries[TCACHE_MAX_TILES];
    uint32_t       entry_count;
    uint32_t       loads_this_frame;  /* TMEM loads (perf counter) */
    uint32_t       hits_this_frame;   /* Already-loaded hits */
} tcache_t;

/* -------------------------------------------------------------------
 * Converted texture storage (in RDRAM)
 *
 * TR1 textures arrive as 256×256 8-bit palettized pages.
 * We pre-convert to N64-friendly formats on level load:
 *   - 4-bit CI tiles (64×64 → 2 KB each, 16 tiles per page)
 *   - Palette entries converted to RGBA5551
 * ------------------------------------------------------------------- */

#define TR_TEX_PAGE_W       256
#define TR_TEX_PAGE_H       256
#define TR_TILES_PER_PAGE   ((TR_TEX_PAGE_W / TR_TILE_W) * (TR_TEX_PAGE_H / TR_TILE_H))
#define MAX_TEX_PAGES       32   /* TR1 has up to ~16 pages */

typedef struct {
    uint8_t  ci4_data[TR_TILE_4BIT_SIZE];  /* 2 KB: 64×64 at 4 bits */
    uint16_t palette[16];                   /* RGBA5551 palette */
    bool     valid;
} n64_tile_t;

typedef struct {
    n64_tile_t tiles[TR_TILES_PER_PAGE];
    uint16_t   full_palette[256];   /* Full 256-color RGBA5551 palette */
    bool       loaded;
} n64_tex_page_t;

/* -------------------------------------------------------------------
 * Vertex format for N64 RDP
 * ------------------------------------------------------------------- */

typedef struct {
    int16_t x, y, z;       /* Screen-space (after transform) */
    int16_t pad;
    int16_t s, t;           /* Texture coordinates (10.5 fixed) */
    uint8_t r, g, b, a;    /* Vertex color/lighting */
} n64_vertex_t;

/* -------------------------------------------------------------------
 * Renderer state
 * ------------------------------------------------------------------- */

typedef struct {
    /* Framebuffers (double-buffered) */
    uint16_t *fb[2];
    uint16_t *zbuf;
    int       current_fb;

    /* RDP command buffer */
    rdp_cmd_buffer_t cmd_buf;

    /* Texture cache */
    tcache_t  tcache;
    n64_tex_page_t *tex_pages;   /* Converted texture pages */
    uint32_t  tex_page_count;

    /* Transform state */
    int32_t   view_matrix[4][4];  /* 16.16 fixed-point */
    int32_t   proj_matrix[4][4];

    /* Per-frame stats */
    uint32_t  tris_drawn;
    uint32_t  tmem_loads;
    uint32_t  draw_calls;
} n64_renderer_t;

/* -------------------------------------------------------------------
 * Public API — implements TRX Output_Draw* interface
 * ------------------------------------------------------------------- */

/* Lifecycle */
void n64_render_init(n64_renderer_t *r);
void n64_render_shutdown(n64_renderer_t *r);

/* Frame */
void n64_render_frame_begin(n64_renderer_t *r);
void n64_render_frame_end(n64_renderer_t *r);   /* Flush RDP, swap buffers */

/* Texture management */
void n64_render_load_tex_pages(n64_renderer_t *r,
                                const uint8_t *pages_8bit,
                                const uint8_t *palette_rgb,
                                uint32_t page_count);
void n64_render_bind_texture(n64_renderer_t *r, uint16_t tex_page, uint16_t tile_idx);

/* Geometry submission */
void n64_render_tri(n64_renderer_t *r, const n64_vertex_t v[3]);
void n64_render_quad(n64_renderer_t *r, const n64_vertex_t v[4]);

/* TRX Output_Draw* implementations */
void n64_draw_room(n64_renderer_t *r, const void *room, bool is_outside);
void n64_draw_object_mesh(n64_renderer_t *r, const void *mesh, int clip);
void n64_draw_sprite(n64_renderer_t *r,
                     int32_t x, int32_t y, int32_t z,
                     int16_t sprnum, int16_t shade);
void n64_draw_shadow(n64_renderer_t *r, int16_t size, const void *bounds, const void *item);

/* UI overlay (uses simple RDP rect commands) */
void n64_draw_screen_flat_quad(n64_renderer_t *r,
                                int32_t sx, int32_t sy, int32_t w, int32_t h,
                                uint32_t color_rgba);
void n64_draw_screen_sprite(n64_renderer_t *r,
                             int32_t sx, int32_t sy, int32_t scale_h, int32_t scale_v,
                             int32_t sprite_idx);

/* Debug overlay */
void n64_render_debug_overlay(n64_renderer_t *r);

/*
 * Lambert fog combiner: vertex color channel used for fog interpolation.
 *
 * When Z-buffer is off and faces are sorted back-to-front, we don't
 * need vertex colors for depth. Instead, encode fog intensity in the
 * vertex color (0=no fog, 255=full fog) and use the combiner to blend:
 *
 *   output = TEXEL0 * (1 - SHADE.a) + FOG_COLOR * SHADE.a
 *
 * This gives per-vertex distance fog at zero extra cost.
 * Fog color is set via SET_FOG_COLOR command.
 */
void n64_render_set_fog_color(n64_renderer_t *r,
                               uint8_t fog_r, uint8_t fog_g, uint8_t fog_b);
void n64_render_set_fog_combiner(n64_renderer_t *r);

#endif /* TRX_N64_RENDER_H */
