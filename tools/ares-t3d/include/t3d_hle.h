/*
 * t3d_hle.h — T3D/rspq High-Level Emulation for ares
 *
 * Intercepts libdragon rspq command streams, identifies T3D overlay
 * commands, and translates them to host GPU calls (OpenGL).
 *
 * Telemetry output is machine-readable, one line per event:
 *   T3D_CMD <name> <key=value pairs>
 *   T3D_FRAME <per-frame summary counters>
 *   T3D_DETECT <overlay detection events>
 *   T3D_WARN <validation warnings>
 *   T3D_ERR <hard errors>
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

/* ----------------------------------------------------------------
 * rspq command format
 * ----------------------------------------------------------------
 * Word 0: [overlay_id(4)][cmd_id(4)][args(24)]
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
 * T3DVertPacked — 32 bytes per 2 verts (big-endian in RDRAM)
 * ---------------------------------------------------------------- */
typedef struct {
    int16_t posA[3];
    uint16_t normA;
    int16_t posB[3];
    uint16_t normB;
    int16_t uvA[2];
    uint8_t rgbaA[4];
    int16_t uvB[2];
    uint8_t rgbaB[4];
} T3DVertPacked_HLE;

_Static_assert(sizeof(T3DVertPacked_HLE) == 32, "T3DVertPacked must be 32 bytes");

/* ----------------------------------------------------------------
 * Vertex (host-side, after endian conversion)
 * ---------------------------------------------------------------- */
#define T3D_HLE_MAX_VERTS   70
#define T3D_HLE_MAX_LIGHTS  8
#define T3D_HLE_MTX_STACK   8

typedef struct {
    float pos[3];
    float uv[2];
    uint8_t rgba[4];
    uint16_t packed_normal;
} T3DHLEVertex;

/* ----------------------------------------------------------------
 * Per-frame counters (machine-readable via T3D_FRAME line)
 * ---------------------------------------------------------------- */
typedef struct {
    uint32_t cmds_total;        /* all commands seen this frame */
    uint32_t cmds_t3d;          /* commands matched to T3D overlay */
    uint32_t cmds_rspq;         /* rspq internal commands */
    uint32_t cmds_other;        /* commands from unknown overlays */
    uint32_t cmds_unsupported;  /* T3D commands we don't handle yet */
    uint32_t verts_loaded;      /* vertices loaded via VERT_LOAD */
    uint32_t tris_drawn;        /* triangles from TRI_DRAW */
    uint32_t tris_invalid;      /* triangles rejected (bad indices) */
    uint32_t tris_strip;        /* triangles from TRI_STRIP */
    uint32_t tris_seq;          /* triangles from TRI_SEQ */
    uint32_t drawcalls;         /* TRI_SYNC count (flush points) */
    uint32_t matrix_ops;        /* MATRIX_STACK operations */
    uint32_t state_changes;     /* DRAWFLAGS + SET_WORD + mode changes */
    uint32_t warnings;          /* validation warnings */
    uint32_t errors;            /* hard errors */
} T3DHLEFrameStats;

/* ----------------------------------------------------------------
 * Overlay detection state
 * ---------------------------------------------------------------- */
#define T3D_DETECT_CONFIDENCE_THRESHOLD 3

typedef struct {
    int overlay_id;             /* -1 = unknown */
    int confidence;             /* number of confirming signals */
    bool confirmed;             /* confidence >= threshold */
    /* Detection signals */
    bool saw_screen_size;
    bool saw_vert_load;
    bool saw_tri_draw;
    bool saw_drawflags;
    bool saw_matrix_stack;
    bool saw_proj_set;
} T3DHLEDetection;

/* ----------------------------------------------------------------
 * Main HLE state
 * ---------------------------------------------------------------- */
typedef struct {
    /* Vertex buffer */
    T3DHLEVertex verts[T3D_HLE_MAX_VERTS];
    int vert_count;

    /* Matrix stack (for future Tier 2/3) */
    int mtx_depth;

    /* Viewport */
    float screen_width, screen_height;

    /* Draw state */
    uint32_t draw_flags;
    bool fog_enabled;

    /* Detection */
    T3DHLEDetection detect;

    /* Per-frame stats (reset each frame) */
    T3DHLEFrameStats frame;

    /* Lifetime stats */
    uint64_t total_frames;
    uint64_t total_cmds;
    uint64_t total_tris;

    /* Config */
    int mode;                   /* 0=telemetry, 1=shadow, 2=replace */
    bool enabled;
    bool verbose;               /* log every command (vs frame summary only) */
    FILE *log_fp;               /* output stream (default: stderr) */
} T3DHLEState;

/* ----------------------------------------------------------------
 * API
 * ---------------------------------------------------------------- */

void t3d_hle_init(T3DHLEState *state, int mode);
void t3d_hle_set_verbose(T3DHLEState *state, bool verbose);
void t3d_hle_set_log(T3DHLEState *state, FILE *fp);

/* Process one rspq command at the given RDRAM address.
 * Returns the command size in words (for the caller to advance),
 * or 0 if this is not a recognized T3D command (fall through). */
int t3d_hle_process_command(T3DHLEState *state,
                            const uint8_t *rdram, uint32_t rdram_size,
                            uint32_t cmd_addr);

/* Signal end of frame. Emits T3D_FRAME summary line. */
void t3d_hle_frame_end(T3DHLEState *state);

/* Query detection state */
bool t3d_hle_is_detected(const T3DHLEState *state);
int  t3d_hle_overlay_id(const T3DHLEState *state);

/* Command name string (for logging) */
const char *t3d_hle_cmd_name(int cmd_id);

/* ----------------------------------------------------------------
 * RDRAM access (big-endian, with bounds checking)
 * ---------------------------------------------------------------- */

static inline uint32_t t3d_hle_read32(const uint8_t *rdram, uint32_t rdram_size, uint32_t addr) {
    addr &= 0x1FFFFFFF; /* physical mask */
    if (addr + 3 >= rdram_size) return 0;
    return ((uint32_t)rdram[addr+0] << 24) |
           ((uint32_t)rdram[addr+1] << 16) |
           ((uint32_t)rdram[addr+2] <<  8) |
           ((uint32_t)rdram[addr+3]);
}

static inline uint16_t t3d_hle_read16(const uint8_t *rdram, uint32_t rdram_size, uint32_t addr) {
    addr &= 0x1FFFFFFF;
    if (addr + 1 >= rdram_size) return 0;
    return ((uint16_t)rdram[addr+0] << 8) | rdram[addr+1];
}

static inline uint8_t t3d_hle_read8(const uint8_t *rdram, uint32_t rdram_size, uint32_t addr) {
    addr &= 0x1FFFFFFF;
    if (addr >= rdram_size) return 0;
    return rdram[addr];
}
