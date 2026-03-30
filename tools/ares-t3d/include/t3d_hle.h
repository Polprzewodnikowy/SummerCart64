/*
 * t3d_hle.h — T3D/rspq High-Level Emulation for ares
 *
 * Intercepts libdragon rspq command streams, identifies T3D overlay
 * commands, and translates them to host GPU calls (OpenGL).
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

/* ----------------------------------------------------------------
 * rspq command format
 * ----------------------------------------------------------------
 * Word 0: [overlay_id(4)][cmd_id(4)][args(24)]
 *   overlay_id: bits 31-28 (0 = internal rspq, 1-15 = registered overlays)
 *   cmd_id:     bits 27-24 (command index within overlay)
 *   args:       bits 23-0  (first argument / flags)
 *
 * Words 1+: additional arguments (command-specific)
 * ---------------------------------------------------------------- */

#define RSPQ_OVL_ID(w0)   (((w0) >> 28) & 0xF)
#define RSPQ_CMD_ID(w0)   (((w0) >> 24) & 0xF)
#define RSPQ_ARGS(w0)     ((w0) & 0x00FFFFFF)

/* rspq internal commands (overlay 0) */
#define RSPQ_CMD_NOOP     0x0
#define RSPQ_CMD_JUMP     0x1
#define RSPQ_CMD_CALL     0x2
#define RSPQ_CMD_RET      0x3
#define RSPQ_CMD_DMA      0x4
#define RSPQ_CMD_WRITE    0x5
#define RSPQ_CMD_TEST     0x6
#define RSPQ_CMD_WSTATUS  0x7

/* ----------------------------------------------------------------
 * T3D command IDs (within T3D overlay)
 * ---------------------------------------------------------------- */
#define T3D_CMD_TRI_DRAW      0x0
#define T3D_CMD_SCREEN_SIZE   0x1
#define T3D_CMD_MATRIX_STACK  0x2
#define T3D_CMD_SET_WORD      0x3
#define T3D_CMD_VERT_LOAD     0x4
#define T3D_CMD_LIGHT_SET     0x5
#define T3D_CMD_DRAWFLAGS     0x6
#define T3D_CMD_PROJ_SET      0x7
#define T3D_CMD_FOG_RANGE     0x8
#define T3D_CMD_FOG_STATE     0x9
#define T3D_CMD_TRI_SYNC      0xA
#define T3D_CMD_TRI_STRIP     0xB
#define T3D_CMD_TRI_SEQ       0xC
#define T3D_CMD_COUNT         13

/* T3D draw flags */
#define T3D_FLAG_DEPTH       (1 << 0)
#define T3D_FLAG_TEXTURED    (1 << 1)
#define T3D_FLAG_SHADED      (1 << 2)
#define T3D_FLAG_CULL_FRONT  (1 << 3)
#define T3D_FLAG_CULL_BACK   (1 << 4)

/* ----------------------------------------------------------------
 * T3DVertPacked — vertex format in RDRAM (32 bytes per 2 verts)
 * Must match libdragon's T3DVertPacked exactly.
 * ---------------------------------------------------------------- */
typedef struct {
    int16_t posA[3];      /* Vertex A: x, y, z */
    uint16_t normA;       /* Vertex A: packed normal */
    int16_t posB[3];      /* Vertex B: x, y, z */
    uint16_t normB;       /* Vertex B: packed normal */
    int16_t uvA[2];       /* Vertex A: s, t */
    uint8_t rgbaA[4];     /* Vertex A: r, g, b, a */
    int16_t uvB[2];       /* Vertex B: s, t */
    uint8_t rgbaB[4];     /* Vertex B: r, g, b, a */
} T3DVertPacked_HLE;

_Static_assert(sizeof(T3DVertPacked_HLE) == 32, "T3DVertPacked must be 32 bytes");

/* ----------------------------------------------------------------
 * T3D HLE state — mirrors T3D's RSP-side state on the host
 * ---------------------------------------------------------------- */

#define T3D_HLE_MAX_VERTS   70
#define T3D_HLE_MAX_LIGHTS  8
#define T3D_HLE_MTX_STACK   8

typedef struct {
    float pos[3];
    float normal[3];
    float uv[2];
    uint8_t rgba[4];
} T3DHLEVertex;

typedef struct {
    float color[4];
    float direction[3];
} T3DHLELight;

typedef struct {
    /* Vertex buffer (transformed by matrices on host) */
    T3DHLEVertex verts[T3D_HLE_MAX_VERTS];
    int vert_count;

    /* Matrix stack */
    float model_mtx[T3D_HLE_MTX_STACK][4][4];
    float proj_mtx[4][4];
    int mtx_depth;

    /* Viewport */
    float screen_width, screen_height;
    float screen_offset_x, screen_offset_y;

    /* Lighting */
    T3DHLELight lights[T3D_HLE_MAX_LIGHTS];
    int light_count;
    uint8_t ambient[4];

    /* Draw state */
    uint32_t draw_flags;
    bool fog_enabled;
    float fog_near, fog_far;

    /* Overlay detection */
    int t3d_overlay_id;     /* -1 = not detected yet */
    int rdpq_overlay_id;    /* -1 = not detected yet */
    bool detected;

    /* Statistics */
    uint64_t cmds_intercepted;
    uint64_t tris_drawn;
    uint64_t frames;

    /* Mode: 0=telemetry, 1=shadow, 2=replace */
    int mode;
    bool enabled;
} T3DHLEState;

/* ----------------------------------------------------------------
 * API — called from ares hook points
 * ---------------------------------------------------------------- */

/* Initialize HLE state. Call once at startup. */
void t3d_hle_init(T3DHLEState *state, int mode);

/* Called when RSP DMA loads data into IMEM — used to detect overlays.
 * Returns true if this looks like rspq/T3D microcode. */
bool t3d_hle_detect_ucode(T3DHLEState *state,
                          const uint8_t *imem, uint32_t size);

/* Called when rspq command buffer is being processed.
 * Reads commands from emulated RDRAM at the given address.
 * Returns true if the command was handled by HLE. */
bool t3d_hle_process_command(T3DHLEState *state,
                             const uint8_t *rdram,
                             uint32_t cmd_addr);

/* Called when a frame is complete (VI interrupt or rdpq_detach_show).
 * In shadow/replace mode, presents the HLE-rendered frame. */
void t3d_hle_frame_end(T3DHLEState *state);

/* Read a 32-bit word from emulated RDRAM */
static inline uint32_t t3d_hle_read32(const uint8_t *rdram, uint32_t addr) {
    addr &= 0x007FFFFF; /* Mask to 8MB RDRAM */
    return ((uint32_t)rdram[addr+0] << 24) |
           ((uint32_t)rdram[addr+1] << 16) |
           ((uint32_t)rdram[addr+2] <<  8) |
           ((uint32_t)rdram[addr+3]);
}

/* Read a 16-bit half from emulated RDRAM */
static inline uint16_t t3d_hle_read16(const uint8_t *rdram, uint32_t addr) {
    addr &= 0x007FFFFF;
    return ((uint16_t)rdram[addr+0] << 8) | rdram[addr+1];
}

/* Command name for logging */
const char *t3d_hle_cmd_name(int cmd_id);
