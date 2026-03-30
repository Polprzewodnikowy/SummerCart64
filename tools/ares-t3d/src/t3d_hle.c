/*
 * t3d_hle.c — T3D/rspq HLE: command parsing, validation, telemetry
 *
 * Tier 1: Parse and log all T3D commands with strict validation.
 * Output is machine-readable (one structured line per event).
 *
 * Output format:
 *   T3D_CMD <name> <key=value ...>
 *   T3D_FRAME frame=N cmds=N t3d=N tris=N verts=N ...
 *   T3D_DETECT event=<type> overlay=N confidence=N
 *   T3D_WARN msg="..." addr=0xNNNN
 *   T3D_ERR msg="..." addr=0xNNNN
 */

#include "t3d_hle.h"
#include <string.h>

/* ----------------------------------------------------------------
 * Command metadata
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

/* Command sizes in 32-bit words (including the command word itself).
 * Verified against t3d.c rspq_write() calls — see COMMAND_REFERENCE.md.
 *
 * Note: rspq_write(ovl, cmd, arg1, arg2, ...) emits cmd_word + N args.
 * Total words = 1 (cmd) + N (args). The sizes below are totals. */
static const int cmd_word_counts[T3D_CMD_COUNT] = {
    [T3D_CMD_TRI_DRAW]     = 2,  /* t3d.c:385 — rspq_write(id, cmd, v0, v12) → but via rdpq_write */
    [T3D_CMD_SCREEN_SIZE]  = 5,  /* t3d.c:518 — rspq_write(id, cmd, gb, off, scl, depth) */
    [T3D_CMD_MATRIX_STACK] = 2,  /* t3d.c:93  — rspq_write(id, cmd, flags, addr) */
    [T3D_CMD_SET_WORD]     = 2,  /* t3d.c:100 — rspq_write(id, cmd, addr, value) */
    [T3D_CMD_VERT_LOAD]    = 3,  /* t3d.c:151 — rspq_write(id, cmd, size, addr, offsets) */
    [T3D_CMD_LIGHT_SET]    = 4,  /* t3d.c:190 — rspq_write(id, cmd, addr, rgba, d1, d2) */
    [T3D_CMD_DRAWFLAGS]    = 2,  /* t3d.c:303 — rspq_write(id, cmd, cullmask, rdpcmd) */
    [T3D_CMD_PROJ_SET]     = 1,  /* t3d.c:134 — rspq_write(id, cmd, addr) */
    [T3D_CMD_FOG_RANGE]    = 2,  /* t3d.c:442 — rspq_write(id, cmd, mul16, offset32) */
    [T3D_CMD_FOG_STATE]    = 1,  /* t3d.h:543 — rspq_write(id, cmd, flag) */
    [T3D_CMD_TRI_SYNC]     = 1,  /* t3d.h:413 — rspq_write(id, cmd, 0) */
    [T3D_CMD_TRI_STRIP]    = 2,  /* t3d.c:425 — rdpq_write(id, cmd, addr, packed) */
    [T3D_CMD_TRI_SEQ]      = 2,  /* t3d.c:404 — rdpq_write(id, cmd, base, end) */
};

const char *t3d_hle_cmd_name(int cmd_id) {
    if (cmd_id >= 0 && cmd_id < T3D_CMD_COUNT) return cmd_names[cmd_id];
    return "UNKNOWN";
}

/* ----------------------------------------------------------------
 * Logging helpers (machine-readable to log_fp)
 * ---------------------------------------------------------------- */

/* Use T3D_LOG from header. Define local alias for brevity. */
#define LOG T3D_LOG

#define LOG_CMD(state, cmd, fmt, ...) \
    do { if ((state)->verbose) \
        fprintf((state)->log_fp, "T3D_CMD %s " fmt "\n", \
                t3d_hle_cmd_name(cmd), ##__VA_ARGS__); \
    } while(0)

#define LOG_WARN(state, fmt, ...) \
    do { (state)->frame.warnings++; \
        fprintf((state)->log_fp, "T3D_WARN " fmt "\n", ##__VA_ARGS__); \
    } while(0)

#define LOG_ERR(state, fmt, ...) \
    do { (state)->frame.errors++; \
        fprintf((state)->log_fp, "T3D_ERR " fmt "\n", ##__VA_ARGS__); \
    } while(0)

/* ----------------------------------------------------------------
 * Read helpers with validation
 * ---------------------------------------------------------------- */

static inline uint32_t rd32(const uint8_t *rdram, uint32_t rdram_size, uint32_t addr) {
    return t3d_hle_read32(rdram, rdram_size, addr);
}

static inline bool addr_valid(uint32_t addr, uint32_t rdram_size) {
    return (addr & 0x1FFFFFFF) + 3 < rdram_size;
}

/* ----------------------------------------------------------------
 * Initialization
 * ---------------------------------------------------------------- */

void t3d_hle_init(T3DHLEState *state, int mode) {
    memset(state, 0, sizeof(*state));
    state->mode = mode;
    state->enabled = true;
    state->verbose = (mode == 0); /* verbose by default in telemetry */
    state->log_fp = stderr;
    state->detect.overlay_id = -1;
    state->detect.confidence = 0;
    state->detect.confirmed = false;

    LOG(state, "T3D_INIT", "mode=%d tier=%s",
        mode, mode == 0 ? "telemetry" : mode == 1 ? "shadow" : "replace");
}

void t3d_hle_set_verbose(T3DHLEState *state, bool verbose) {
    state->verbose = verbose;
}

void t3d_hle_set_log(T3DHLEState *state, FILE *fp) {
    state->log_fp = fp ? fp : stderr;
}

/* ----------------------------------------------------------------
 * Overlay detection — multi-signal confidence scoring
 *
 * We observe commands from each non-zero overlay ID. When a command
 * matches a known T3D signature (SCREEN_SIZE params, VERT_LOAD with
 * valid RDRAM, TRI_DRAW indices, etc.), we increment confidence for
 * that overlay. Once confidence crosses the threshold, we lock it in.
 * ---------------------------------------------------------------- */

static void detect_signal(T3DHLEState *state, int ovl_id, const char *signal) {
    T3DHLEDetection *d = &state->detect;

    /* If already confirmed for a different overlay, ignore */
    if (d->confirmed && d->overlay_id != ovl_id) return;

    /* First signal sets candidate overlay */
    if (d->overlay_id == -1) {
        d->overlay_id = ovl_id;
        LOG(state, "T3D_DETECT", "event=candidate overlay=%d signal=%s", ovl_id, signal);
    }

    /* Only count signals for the candidate */
    if (d->overlay_id != ovl_id) return;

    d->confidence++;
    LOG(state, "T3D_DETECT", "event=signal overlay=%d signal=%s confidence=%d/%d",
        ovl_id, signal, d->confidence, T3D_DETECT_CONFIDENCE_THRESHOLD);

    if (!d->confirmed && d->confidence >= T3D_DETECT_CONFIDENCE_THRESHOLD) {
        d->confirmed = true;
        LOG(state, "T3D_DETECT", "event=confirmed overlay=%d confidence=%d",
            ovl_id, d->confidence);
    }
}

/* ----------------------------------------------------------------
 * Command handlers
 * ---------------------------------------------------------------- */

static int handle_vert_load(T3DHLEState *state,
                            const uint8_t *rdram, uint32_t rdram_size,
                            uint32_t cmd_addr, uint32_t w0) {
    uint32_t w1 = rd32(rdram, rdram_size, cmd_addr + 4);
    uint32_t w2 = rd32(rdram, rdram_size, cmd_addr + 8);

    uint32_t input_size = RSPQ_ARGS(w0);
    uint32_t phys_addr = w1 & 0x1FFFFFFF;
    uint16_t dest_off  = (w2 >> 16) & 0xFFFF;
    uint16_t input_off = w2 & 0xFFFF;

    int num_packed = input_size / 32;
    int num_verts = num_packed * 2;

    /* Validate */
    if (num_verts > T3D_HLE_MAX_VERTS) {
        LOG_WARN(state, "msg=\"VERT_LOAD count exceeds max\" count=%d max=%d addr=0x%08X",
                 num_verts, T3D_HLE_MAX_VERTS, phys_addr);
        num_verts = T3D_HLE_MAX_VERTS;
        num_packed = num_verts / 2;
    }
    if (num_verts == 0) {
        LOG_WARN(state, "msg=\"VERT_LOAD zero vertices\" input_size=%u addr=0x%08X",
                 input_size, phys_addr);
        return 3;
    }
    if (!addr_valid(phys_addr, rdram_size) ||
        !addr_valid(phys_addr + num_packed * 32 - 1, rdram_size)) {
        LOG_ERR(state, "msg=\"VERT_LOAD addr out of RDRAM\" addr=0x%08X size=%u rdram_size=%u",
                phys_addr, num_packed * 32, rdram_size);
        return 3;
    }

    /* Read vertex data from RDRAM (big-endian) */
    for (int i = 0; i < num_packed; i++) {
        uint32_t a = phys_addr + i * 32;
        int vi = i * 2;

        if (vi < T3D_HLE_MAX_VERTS) {
            T3DHLEVertex *v = &state->verts[vi];
            v->pos[0] = (float)(int16_t)t3d_hle_read16(rdram, rdram_size, a + 0);
            v->pos[1] = (float)(int16_t)t3d_hle_read16(rdram, rdram_size, a + 2);
            v->pos[2] = (float)(int16_t)t3d_hle_read16(rdram, rdram_size, a + 4);
            v->packed_normal = t3d_hle_read16(rdram, rdram_size, a + 6);
            v->uv[0]  = (float)(int16_t)t3d_hle_read16(rdram, rdram_size, a + 16);
            v->uv[1]  = (float)(int16_t)t3d_hle_read16(rdram, rdram_size, a + 18);
            v->rgba[0] = t3d_hle_read8(rdram, rdram_size, a + 20);
            v->rgba[1] = t3d_hle_read8(rdram, rdram_size, a + 21);
            v->rgba[2] = t3d_hle_read8(rdram, rdram_size, a + 22);
            v->rgba[3] = t3d_hle_read8(rdram, rdram_size, a + 23);
        }
        if (vi + 1 < T3D_HLE_MAX_VERTS) {
            T3DHLEVertex *v = &state->verts[vi + 1];
            v->pos[0] = (float)(int16_t)t3d_hle_read16(rdram, rdram_size, a + 8);
            v->pos[1] = (float)(int16_t)t3d_hle_read16(rdram, rdram_size, a + 10);
            v->pos[2] = (float)(int16_t)t3d_hle_read16(rdram, rdram_size, a + 12);
            v->packed_normal = t3d_hle_read16(rdram, rdram_size, a + 14);
            v->uv[0]  = (float)(int16_t)t3d_hle_read16(rdram, rdram_size, a + 24);
            v->uv[1]  = (float)(int16_t)t3d_hle_read16(rdram, rdram_size, a + 26);
            v->rgba[0] = t3d_hle_read8(rdram, rdram_size, a + 28);
            v->rgba[1] = t3d_hle_read8(rdram, rdram_size, a + 29);
            v->rgba[2] = t3d_hle_read8(rdram, rdram_size, a + 30);
            v->rgba[3] = t3d_hle_read8(rdram, rdram_size, a + 31);
        }
    }
    state->vert_count = num_verts;
    state->frame.verts_loaded += num_verts;

    LOG_CMD(state, T3D_CMD_VERT_LOAD,
            "addr=0x%08X verts=%d dest_off=%u input_off=%u",
            phys_addr, num_verts, dest_off, input_off);

    return 3; /* 3 words consumed */
}

static int handle_tri_draw(T3DHLEState *state,
                           const uint8_t *rdram, uint32_t rdram_size,
                           uint32_t cmd_addr, uint32_t w0) {
    /* TRI_DRAW is 2 words:
     * Word 0: [ovl|cmd|v0_dmem_addr(24)]
     * Word 1: [v1_dmem_addr(16) | v2_dmem_addr(16)]
     *
     * The addresses are DMEM offsets into the T3D vertex output buffer,
     * not raw vertex indices. To convert to vertex indices, we'd need
     * to know RSP_T3D_VERT_BUFFER offset and VERT_OUTPUT_SIZE.
     * For telemetry, log the raw DMEM addresses.
     * For the HLE vertex buffer (which uses 0-based indices), we
     * approximate: index = dmem_offset / VERT_OUTPUT_SIZE. */
    uint32_t w1 = rd32(rdram, rdram_size, cmd_addr + 4);
    uint32_t v0_addr = RSPQ_ARGS(w0);
    uint32_t v1_addr = (w1 >> 16) & 0xFFFF;
    uint32_t v2_addr = w1 & 0xFFFF;

    /* T3D vertex output size is 36 bytes per vertex in DMEM.
     * Vertex buffer starts at a known DMEM offset.
     * For telemetry, just log the raw addresses. */
    state->frame.tris_drawn++;
    LOG_CMD(state, T3D_CMD_TRI_DRAW,
            "v0_addr=0x%06X v1_addr=0x%04X v2_addr=0x%04X",
            v0_addr, v1_addr, v2_addr);
    return 2;
}

static int handle_screen_size(T3DHLEState *state,
                              const uint8_t *rdram, uint32_t rdram_size,
                              uint32_t cmd_addr, uint32_t w0) {
    (void)w0;
    uint32_t w1 = rd32(rdram, rdram_size, cmd_addr + 4);
    uint32_t w2 = rd32(rdram, rdram_size, cmd_addr + 8);
    uint32_t w3 = rd32(rdram, rdram_size, cmd_addr + 12);
    uint32_t w4 = rd32(rdram, rdram_size, cmd_addr + 16);

    LOG_CMD(state, T3D_CMD_SCREEN_SIZE,
            "guardband=0x%08X offset=0x%08X scale=0x%08X depth=0x%08X",
            w1, w2, w3, w4);

    state->frame.state_changes++;
    return 5;
}

static int handle_drawflags(T3DHLEState *state,
                            const uint8_t *rdram, uint32_t rdram_size,
                            uint32_t cmd_addr, uint32_t w0) {
    /* DRAWFLAGS is 2 words:
     * Word 0: [ovl|cmd|cull_mask(24)]
     * Word 1: RDP command word (flags packed for rendering mode) */
    uint32_t w1 = rd32(rdram, rdram_size, cmd_addr + 4);
    uint32_t cull_mask = RSPQ_ARGS(w0);
    state->draw_flags = cull_mask;
    state->frame.state_changes++;
    LOG_CMD(state, T3D_CMD_DRAWFLAGS,
            "cull_mask=0x%06X rdp_cmd=0x%08X",
            cull_mask, w1);
    return 2;
}

static int handle_matrix_stack(T3DHLEState *state,
                               const uint8_t *rdram, uint32_t rdram_size,
                               uint32_t cmd_addr, uint32_t w0) {
    uint32_t w1 = rd32(rdram, rdram_size, cmd_addr + 4);
    uint32_t flags = RSPQ_ARGS(w0);
    uint32_t mtx_addr = w1 & 0x1FFFFFFF;

    bool multiply = !!(flags & 1);
    bool push = !!(flags & 2);

    /* Validate matrix address */
    if (mtx_addr != 0 && !addr_valid(mtx_addr + 63, rdram_size)) {
        LOG_WARN(state, "msg=\"MATRIX_STACK addr out of RDRAM\" addr=0x%08X", mtx_addr);
    }

    state->frame.matrix_ops++;
    LOG_CMD(state, T3D_CMD_MATRIX_STACK,
            "addr=0x%08X flags=0x%06X multiply=%d push=%d",
            mtx_addr, flags, multiply, push);
    return 2;
}

static int handle_proj_set(T3DHLEState *state,
                           uint32_t w0) {
    /* PROJ_SET is 1 word: [ovl|cmd|rdram_addr(24)]
     * The address is in the 24-bit args field (PhysicalAddr). */
    uint32_t mtx_addr = RSPQ_ARGS(w0);

    state->frame.state_changes++;
    LOG_CMD(state, T3D_CMD_PROJ_SET, "addr=0x%06X", mtx_addr);
    return 1;
}

static int handle_set_word(T3DHLEState *state,
                           const uint8_t *rdram, uint32_t rdram_size,
                           uint32_t cmd_addr, uint32_t w0) {
    uint32_t w1 = rd32(rdram, rdram_size, cmd_addr + 4);
    uint32_t dmem_addr = RSPQ_ARGS(w0);
    uint32_t value = w1;

    state->frame.state_changes++;
    LOG_CMD(state, T3D_CMD_SET_WORD, "dmem=0x%06X value=0x%08X", dmem_addr, value);
    return 2;
}

static int handle_light_set(T3DHLEState *state,
                            const uint8_t *rdram, uint32_t rdram_size,
                            uint32_t cmd_addr, uint32_t w0) {
    uint32_t w1 = rd32(rdram, rdram_size, cmd_addr + 4);
    uint32_t w2 = rd32(rdram, rdram_size, cmd_addr + 8);
    uint32_t w3 = rd32(rdram, rdram_size, cmd_addr + 12);

    state->frame.state_changes++;
    LOG_CMD(state, T3D_CMD_LIGHT_SET,
            "w0=0x%08X w1=0x%08X w2=0x%08X w3=0x%08X", w0, w1, w2, w3);
    return 4;
}

static int handle_fog_range(T3DHLEState *state,
                            const uint8_t *rdram, uint32_t rdram_size,
                            uint32_t cmd_addr, uint32_t w0) {
    uint32_t w1 = rd32(rdram, rdram_size, cmd_addr + 4);
    state->frame.state_changes++;
    LOG_CMD(state, T3D_CMD_FOG_RANGE, "w0=0x%08X w1=0x%08X", w0, w1);
    return 2;
}

static int handle_fog_state(T3DHLEState *state, uint32_t w0) {
    uint32_t flags = RSPQ_ARGS(w0);
    state->fog_enabled = !!(flags & 1);
    state->frame.state_changes++;
    LOG_CMD(state, T3D_CMD_FOG_STATE, "enabled=%d", state->fog_enabled);
    return 1;
}

static int handle_tri_sync(T3DHLEState *state) {
    state->frame.drawcalls++;
    LOG_CMD(state, T3D_CMD_TRI_SYNC, "drawcall=%u", state->frame.drawcalls);
    return 1;
}

static int handle_tri_strip(T3DHLEState *state,
                            const uint8_t *rdram, uint32_t rdram_size,
                            uint32_t cmd_addr, uint32_t w0) {
    uint32_t w1 = rd32(rdram, rdram_size, cmd_addr + 4);
    /* TODO: decode strip indices from packed words */
    state->frame.cmds_unsupported++;
    LOG_CMD(state, T3D_CMD_TRI_STRIP, "w0=0x%08X w1=0x%08X", w0, w1);
    return 2;
}

static int handle_tri_seq(T3DHLEState *state,
                          const uint8_t *rdram, uint32_t rdram_size,
                          uint32_t cmd_addr, uint32_t w0) {
    uint32_t w1 = rd32(rdram, rdram_size, cmd_addr + 4);
    /* TODO: decode sequential tri indices from packed words */
    state->frame.cmds_unsupported++;
    LOG_CMD(state, T3D_CMD_TRI_SEQ, "w0=0x%08X w1=0x%08X", w0, w1);
    return 2;
}

/* ----------------------------------------------------------------
 * Main command processor
 * ---------------------------------------------------------------- */

int t3d_hle_process_command(T3DHLEState *state,
                            const uint8_t *rdram, uint32_t rdram_size,
                            uint32_t cmd_addr) {
    if (!state->enabled) return 0;

    /* Bounds check */
    if (!addr_valid(cmd_addr, rdram_size)) {
        LOG_ERR(state, "msg=\"cmd_addr out of RDRAM\" addr=0x%08X", cmd_addr);
        return 0;
    }

    uint32_t w0 = rd32(rdram, rdram_size, cmd_addr);
    int ovl_id = RSPQ_OVL_ID(w0);
    int cmd_id = RSPQ_CMD_ID(w0);

    state->frame.cmds_total++;

    /* Internal rspq commands (overlay 0) — count but don't process */
    if (ovl_id == 0) {
        state->frame.cmds_rspq++;
        return 0; /* let ares handle these */
    }

    /* Detection: accumulate signals from command patterns */
    T3DHLEDetection *d = &state->detect;
    if (!d->confirmed) {
        /* Only consider as T3D candidate if cmd_id is in valid range
         * and the overlay matches our candidate (or we have none yet) */
        if (cmd_id < T3D_CMD_COUNT && (d->overlay_id == -1 || d->overlay_id == ovl_id)) {
            switch (cmd_id) {
            case T3D_CMD_SCREEN_SIZE:
                if (!d->saw_screen_size) { d->saw_screen_size = true; detect_signal(state, ovl_id, "SCREEN_SIZE"); }
                break;
            case T3D_CMD_VERT_LOAD: {
                /* Extra validation: word 1 should be a valid RDRAM address */
                uint32_t w1 = rd32(rdram, rdram_size, cmd_addr + 4);
                uint32_t pa = w1 & 0x1FFFFFFF;
                if (pa > 0 && pa < rdram_size) {
                    if (!d->saw_vert_load) { d->saw_vert_load = true; detect_signal(state, ovl_id, "VERT_LOAD_valid_addr"); }
                }
                break;
            }
            case T3D_CMD_TRI_DRAW:
                if (!d->saw_tri_draw) { d->saw_tri_draw = true; detect_signal(state, ovl_id, "TRI_DRAW"); }
                break;
            case T3D_CMD_DRAWFLAGS:
                if (!d->saw_drawflags) { d->saw_drawflags = true; detect_signal(state, ovl_id, "DRAWFLAGS"); }
                break;
            case T3D_CMD_MATRIX_STACK:
                if (!d->saw_matrix_stack) { d->saw_matrix_stack = true; detect_signal(state, ovl_id, "MATRIX_STACK"); }
                break;
            case T3D_CMD_PROJ_SET:
                if (!d->saw_proj_set) { d->saw_proj_set = true; detect_signal(state, ovl_id, "PROJ_SET"); }
                break;
            }
        }
    }

    /* If this overlay isn't our confirmed or candidate T3D, skip */
    if (d->overlay_id != ovl_id) {
        state->frame.cmds_other++;
        return 0;
    }

    /* Even if not yet confirmed, process commands from candidate overlay
     * (telemetry mode — we want to see everything) */
    state->frame.cmds_t3d++;
    state->total_cmds++;

    /* Validate command ID range */
    if (cmd_id >= T3D_CMD_COUNT) {
        LOG_WARN(state, "msg=\"unknown T3D cmd\" cmd_id=%d overlay=%d addr=0x%08X",
                 cmd_id, ovl_id, cmd_addr);
        state->frame.cmds_unsupported++;
        return 1; /* assume 1 word, caller should handle gracefully */
    }

    /* Validate we can read the full command */
    int expected_words = cmd_word_counts[cmd_id];
    if (expected_words > 1 && !addr_valid(cmd_addr + (expected_words - 1) * 4, rdram_size)) {
        LOG_ERR(state, "msg=\"cmd extends past RDRAM\" cmd=%s words=%d addr=0x%08X",
                t3d_hle_cmd_name(cmd_id), expected_words, cmd_addr);
        return 1;
    }

    /* Dispatch */
    switch (cmd_id) {
    case T3D_CMD_VERT_LOAD:    return handle_vert_load(state, rdram, rdram_size, cmd_addr, w0);
    case T3D_CMD_TRI_DRAW:     return handle_tri_draw(state, rdram, rdram_size, cmd_addr, w0);
    case T3D_CMD_SCREEN_SIZE:  return handle_screen_size(state, rdram, rdram_size, cmd_addr, w0);
    case T3D_CMD_DRAWFLAGS:    return handle_drawflags(state, rdram, rdram_size, cmd_addr, w0);
    case T3D_CMD_MATRIX_STACK: return handle_matrix_stack(state, rdram, rdram_size, cmd_addr, w0);
    case T3D_CMD_PROJ_SET:     return handle_proj_set(state, w0);
    case T3D_CMD_SET_WORD:     return handle_set_word(state, rdram, rdram_size, cmd_addr, w0);
    case T3D_CMD_LIGHT_SET:    return handle_light_set(state, rdram, rdram_size, cmd_addr, w0);
    case T3D_CMD_FOG_RANGE:    return handle_fog_range(state, rdram, rdram_size, cmd_addr, w0);
    case T3D_CMD_FOG_STATE:    return handle_fog_state(state, w0);
    case T3D_CMD_TRI_SYNC:     return handle_tri_sync(state);
    case T3D_CMD_TRI_STRIP:    return handle_tri_strip(state, rdram, rdram_size, cmd_addr, w0);
    case T3D_CMD_TRI_SEQ:      return handle_tri_seq(state, rdram, rdram_size, cmd_addr, w0);
    default:
        state->frame.cmds_unsupported++;
        return expected_words > 0 ? expected_words : 1;
    }
}

/* ----------------------------------------------------------------
 * Frame end — emit machine-readable summary
 * ---------------------------------------------------------------- */

void t3d_hle_frame_end(T3DHLEState *state) {
    T3DHLEFrameStats *f = &state->frame;

    state->total_frames++;
    state->total_tris += f->tris_drawn;

    fprintf(state->log_fp,
            "T3D_FRAME frame=%llu cmds=%u t3d=%u rspq=%u other=%u unsup=%u "
            "verts=%u tris=%u tris_invalid=%u tris_strip=%u tris_seq=%u "
            "drawcalls=%u mtx_ops=%u state_chg=%u warn=%u err=%u "
            "overlay=%d confirmed=%d\n",
            (unsigned long long)state->total_frames,
            f->cmds_total, f->cmds_t3d, f->cmds_rspq, f->cmds_other,
            f->cmds_unsupported,
            f->verts_loaded, f->tris_drawn, f->tris_invalid,
            f->tris_strip, f->tris_seq,
            f->drawcalls, f->matrix_ops, f->state_changes,
            f->warnings, f->errors,
            state->detect.overlay_id, state->detect.confirmed);

    /* Reset per-frame counters */
    memset(f, 0, sizeof(*f));
}

/* ----------------------------------------------------------------
 * Query
 * ---------------------------------------------------------------- */

bool t3d_hle_is_detected(const T3DHLEState *state) {
    return state->detect.confirmed;
}

int t3d_hle_overlay_id(const T3DHLEState *state) {
    return state->detect.overlay_id;
}
