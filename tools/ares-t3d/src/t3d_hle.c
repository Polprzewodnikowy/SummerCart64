/*
 * t3d_hle.c — T3D/rspq HLE core: command parsing and state tracking
 *
 * Tier 1: Telemetry — parse and log all T3D commands
 * Tier 2: Shadow — also render via host GL
 * Tier 3: Replace — skip RSP/RDP entirely for known paths
 */

#include "t3d_hle.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ----------------------------------------------------------------
 * Command names (for telemetry logging)
 * ---------------------------------------------------------------- */
static const char *cmd_names[T3D_CMD_COUNT] = {
    [T3D_CMD_TRI_DRAW]     = "TRI_DRAW",
    [T3D_CMD_SCREEN_SIZE]  = "SCREEN_SIZE",
    [T3D_CMD_MATRIX_STACK] = "MATRIX_STACK",
    [T3D_CMD_SET_WORD]     = "SET_WORD",
    [T3D_CMD_VERT_LOAD]    = "VERT_LOAD",
    [T3D_CMD_LIGHT_SET]    = "LIGHT_SET",
    [T3D_CMD_DRAWFLAGS]    = "DRAWFLAGS",
    [T3D_CMD_PROJ_SET]     = "PROJ_SET",
    [T3D_CMD_FOG_RANGE]    = "FOG_RANGE",
    [T3D_CMD_FOG_STATE]    = "FOG_STATE",
    [T3D_CMD_TRI_SYNC]     = "TRI_SYNC",
    [T3D_CMD_TRI_STRIP]    = "TRI_STRIP",
    [T3D_CMD_TRI_SEQ]      = "TRI_SEQ",
};

const char *t3d_hle_cmd_name(int cmd_id) {
    if (cmd_id >= 0 && cmd_id < T3D_CMD_COUNT) return cmd_names[cmd_id];
    return "UNKNOWN";
}

/* ----------------------------------------------------------------
 * Initialization
 * ---------------------------------------------------------------- */
void t3d_hle_init(T3DHLEState *state, int mode) {
    memset(state, 0, sizeof(*state));
    state->mode = mode;
    state->enabled = true;
    state->t3d_overlay_id = -1;
    state->rdpq_overlay_id = -1;
    state->detected = false;
    state->mtx_depth = 0;

    /* Identity matrices */
    for (int s = 0; s < T3D_HLE_MTX_STACK; s++) {
        for (int i = 0; i < 4; i++)
            state->model_mtx[s][i][i] = 1.0f;
    }
    for (int i = 0; i < 4; i++)
        state->proj_mtx[i][i] = 1.0f;

    fprintf(stderr, "[T3D-HLE] Initialized (mode=%d: %s)\n", mode,
            mode == 0 ? "telemetry" : mode == 1 ? "shadow" : "replace");
}

/* ----------------------------------------------------------------
 * Overlay detection
 *
 * rspq overlays are identified by their IMEM code signature.
 * T3D's overlay text starts with a known pattern. We also detect
 * the overlay ID at runtime by watching rspq CALL/DMA commands
 * that reference overlay table entries.
 *
 * Simpler approach: watch for T3D_CMD patterns in the command
 * stream. If overlay N issues commands matching T3D's known
 * command set (VERT_LOAD with valid RDRAM addresses, TRI_DRAW
 * with valid vertex indices, SCREEN_SIZE with 320/240), we
 * fingerprint it as T3D.
 * ---------------------------------------------------------------- */

/* Known rspq overlay text signatures (first 8 bytes of overlay code) */
static const uint8_t t3d_sig[] = {
    /* rsp_tiny3d text segment starts with specific instructions.
     * We match a hash of the first 64 bytes instead of exact bytes
     * to handle minor version differences. */
};

bool t3d_hle_detect_ucode(T3DHLEState *state,
                          const uint8_t *imem, uint32_t size) {
    /* For now, detection happens via command pattern matching
     * rather than IMEM signature. See process_command(). */
    (void)imem; (void)size;
    return false;
}

/* ----------------------------------------------------------------
 * Command processing — the core of Tier 1
 *
 * Called when we observe rspq commands in RDRAM. Parses the
 * overlay_id and cmd_id, updates internal state, and logs.
 * ---------------------------------------------------------------- */

/* rspq command sizes (in 32-bit words) for T3D commands.
 * Derived from T3D's rspq overlay descriptor table. */
static const int t3d_cmd_sizes[T3D_CMD_COUNT] = {
    [T3D_CMD_TRI_DRAW]     = 1,  /* packed indices in word 0 */
    [T3D_CMD_SCREEN_SIZE]  = 5,  /* guardband, offset, scale, depth */
    [T3D_CMD_MATRIX_STACK] = 2,  /* address + flags */
    [T3D_CMD_SET_WORD]     = 2,  /* dmem addr + value */
    [T3D_CMD_VERT_LOAD]    = 3,  /* size, rdram addr, offsets */
    [T3D_CMD_LIGHT_SET]    = 4,  /* color + direction */
    [T3D_CMD_DRAWFLAGS]    = 1,  /* flags in args */
    [T3D_CMD_PROJ_SET]     = 2,  /* address */
    [T3D_CMD_FOG_RANGE]    = 1,  /* packed range */
    [T3D_CMD_FOG_STATE]    = 1,  /* enable/disable */
    [T3D_CMD_TRI_SYNC]     = 1,  /* no args */
    [T3D_CMD_TRI_STRIP]    = 2,  /* indices packed */
    [T3D_CMD_TRI_SEQ]      = 2,  /* indices packed */
};

static void log_command(T3DHLEState *state, int ovl_id, int cmd_id,
                        const uint32_t *words, int nwords) {
    if (state->mode > 0) return; /* Only log in telemetry mode */

    if (ovl_id == state->t3d_overlay_id && cmd_id < T3D_CMD_COUNT) {
        fprintf(stderr, "[T3D] %s", t3d_hle_cmd_name(cmd_id));
        for (int i = 0; i < nwords && i < 4; i++)
            fprintf(stderr, " %08X", words[i]);
        fprintf(stderr, "\n");
    }
}

static void handle_t3d_vert_load(T3DHLEState *state,
                                 const uint8_t *rdram,
                                 const uint32_t *words) {
    /* Word 0: [ovl_id|cmd_id|input_size(24)]
     * Word 1: RDRAM physical address of T3DVertPacked array
     * Word 2: [dest_offset(16)|input_offset(16)] */
    uint32_t input_size = RSPQ_ARGS(words[0]);
    uint32_t rdram_addr = words[1] & 0x00FFFFFF; /* Physical addr mask */
    uint16_t dest_off   = (words[2] >> 16) & 0xFFFF;
    uint16_t input_off  = words[2] & 0xFFFF;

    /* Calculate number of vertices (each T3DVertPacked holds 2 verts) */
    int num_packed = input_size / 32;
    int num_verts = num_packed * 2;
    if (num_verts > T3D_HLE_MAX_VERTS) num_verts = T3D_HLE_MAX_VERTS;

    /* Read vertex data from emulated RDRAM */
    for (int i = 0; i < num_packed; i++) {
        uint32_t addr = rdram_addr + i * 32;
        T3DVertPacked_HLE packed;
        memcpy(&packed, rdram + (addr & 0x007FFFFF), sizeof(packed));

        /* Byte-swap from big-endian N64 to host */
        int vi = i * 2;
        if (vi < T3D_HLE_MAX_VERTS) {
            state->verts[vi].pos[0] = (float)(int16_t)__builtin_bswap16(packed.posA[0]);
            state->verts[vi].pos[1] = (float)(int16_t)__builtin_bswap16(packed.posA[1]);
            state->verts[vi].pos[2] = (float)(int16_t)__builtin_bswap16(packed.posA[2]);
            state->verts[vi].uv[0]  = (float)(int16_t)__builtin_bswap16(packed.uvA[0]);
            state->verts[vi].uv[1]  = (float)(int16_t)__builtin_bswap16(packed.uvA[1]);
            state->verts[vi].rgba[0] = packed.rgbaA[0];
            state->verts[vi].rgba[1] = packed.rgbaA[1];
            state->verts[vi].rgba[2] = packed.rgbaA[2];
            state->verts[vi].rgba[3] = packed.rgbaA[3];
        }
        if (vi + 1 < T3D_HLE_MAX_VERTS) {
            state->verts[vi+1].pos[0] = (float)(int16_t)__builtin_bswap16(packed.posB[0]);
            state->verts[vi+1].pos[1] = (float)(int16_t)__builtin_bswap16(packed.posB[1]);
            state->verts[vi+1].pos[2] = (float)(int16_t)__builtin_bswap16(packed.posB[2]);
            state->verts[vi+1].uv[0]  = (float)(int16_t)__builtin_bswap16(packed.uvB[0]);
            state->verts[vi+1].uv[1]  = (float)(int16_t)__builtin_bswap16(packed.uvB[1]);
            state->verts[vi+1].rgba[0] = packed.rgbaB[0];
            state->verts[vi+1].rgba[1] = packed.rgbaB[1];
            state->verts[vi+1].rgba[2] = packed.rgbaB[2];
            state->verts[vi+1].rgba[3] = packed.rgbaB[3];
        }
    }
    state->vert_count = num_verts;
}

static void handle_t3d_tri_draw(T3DHLEState *state,
                                const uint32_t *words) {
    /* Word 0: [ovl_id|cmd_id|v0(8)|v1(8)|v2(8)] */
    uint32_t args = RSPQ_ARGS(words[0]);
    int v0 = (args >> 16) & 0xFF;
    int v1 = (args >>  8) & 0xFF;
    int v2 = (args >>  0) & 0xFF;

    if (v0 >= state->vert_count || v1 >= state->vert_count || v2 >= state->vert_count)
        return;

    state->tris_drawn++;

    /* In replace mode, this is where we'd issue GL draw calls */
    if (state->mode == 2) {
        /* TODO: Tier 3 — glDrawArrays or equivalent */
    }
}

static void handle_t3d_drawflags(T3DHLEState *state,
                                 const uint32_t *words) {
    state->draw_flags = RSPQ_ARGS(words[0]);
}

static void handle_t3d_matrix_set(T3DHLEState *state,
                                  const uint8_t *rdram,
                                  const uint32_t *words) {
    /* Word 0: [ovl_id|cmd_id|flags(24)]
     * Word 1: RDRAM address of T3DMat4FP (64 bytes, fixed-point)
     *
     * Flags:
     *   bit 0: multiply with parent (1) or replace (0)
     *   bit 1: push (1) or set (0)
     *   bit 2: pop direction */
    uint32_t flags = RSPQ_ARGS(words[0]);
    uint32_t mtx_addr = words[1] & 0x00FFFFFF;

    /* Read fixed-point matrix from RDRAM and convert to float.
     * T3DMat4FP is 16x int16_t integer + 16x uint16_t fraction = 64 bytes.
     * Layout: first 32 bytes = integer parts, next 32 bytes = fraction parts */
    /* TODO: Implement fixed-point to float matrix conversion */

    (void)flags; (void)mtx_addr; (void)rdram;
}

static void handle_t3d_screen_size(T3DHLEState *state,
                                   const uint32_t *words) {
    /* Word 1: guardband scale + UV gen offset
     * Word 2: screen offset (x<<17 | y<<1)
     * Word 3: screen scale (x<<16 | y)
     * Word 4: depth + W scale */
    /* TODO: Extract viewport dimensions */
    (void)words;
    state->screen_width = 320.0f;  /* Default */
    state->screen_height = 240.0f;
}

bool t3d_hle_process_command(T3DHLEState *state,
                             const uint8_t *rdram,
                             uint32_t cmd_addr) {
    if (!state->enabled) return false;

    uint32_t w0 = t3d_hle_read32(rdram, cmd_addr);
    int ovl_id = RSPQ_OVL_ID(w0);
    int cmd_id = RSPQ_CMD_ID(w0);

    /* Overlay detection via pattern matching.
     * If we see SCREEN_SIZE with reasonable values followed by
     * VERT_LOAD with valid RDRAM addresses, fingerprint as T3D. */
    if (!state->detected && ovl_id > 0 && ovl_id < 16) {
        if (cmd_id == T3D_CMD_SCREEN_SIZE) {
            /* Tentatively mark this overlay as T3D */
            state->t3d_overlay_id = ovl_id;
            state->detected = true;
            fprintf(stderr, "[T3D-HLE] Detected T3D overlay ID: %d\n", ovl_id);
        }
    }

    /* Not our overlay — skip */
    if (ovl_id != state->t3d_overlay_id) return false;

    /* Read additional command words */
    int nwords = (cmd_id < T3D_CMD_COUNT) ? t3d_cmd_sizes[cmd_id] : 1;
    uint32_t words[16];
    words[0] = w0;
    for (int i = 1; i < nwords && i < 16; i++)
        words[i] = t3d_hle_read32(rdram, cmd_addr + i * 4);

    state->cmds_intercepted++;

    /* Log in telemetry mode */
    log_command(state, ovl_id, cmd_id, words, nwords);

    /* Dispatch */
    switch (cmd_id) {
        case T3D_CMD_VERT_LOAD:
            handle_t3d_vert_load(state, rdram, words);
            break;
        case T3D_CMD_TRI_DRAW:
            handle_t3d_tri_draw(state, words);
            break;
        case T3D_CMD_DRAWFLAGS:
            handle_t3d_drawflags(state, words);
            break;
        case T3D_CMD_MATRIX_STACK:
            handle_t3d_matrix_set(state, rdram, words);
            break;
        case T3D_CMD_SCREEN_SIZE:
            handle_t3d_screen_size(state, words);
            break;
        case T3D_CMD_TRI_SYNC:
            /* Flush pending triangles to GL */
            break;
        case T3D_CMD_PROJ_SET:
        case T3D_CMD_LIGHT_SET:
        case T3D_CMD_SET_WORD:
        case T3D_CMD_FOG_RANGE:
        case T3D_CMD_FOG_STATE:
        case T3D_CMD_TRI_STRIP:
        case T3D_CMD_TRI_SEQ:
            /* TODO: implement remaining commands */
            break;
    }

    /* In replace mode (Tier 3), return true to tell ares to skip RSP */
    return (state->mode == 2);
}

void t3d_hle_frame_end(T3DHLEState *state) {
    state->frames++;

    /* Print stats every 60 frames */
    if ((state->frames % 60) == 0) {
        fprintf(stderr, "[T3D-HLE] Frame %llu: %llu cmds, %llu tris "
                "(overlay=%d, mode=%d)\n",
                (unsigned long long)state->frames,
                (unsigned long long)state->cmds_intercepted,
                (unsigned long long)state->tris_drawn,
                state->t3d_overlay_id,
                state->mode);
    }
}
