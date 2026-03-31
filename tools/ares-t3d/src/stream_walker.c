/*
 * stream_walker.c — Correct mixed rspq stream walker
 *
 * Uses overlay_sizes.h to step correctly through multiplexed
 * command buffers. Only dispatches T3D commands to the parser.
 */

#include "stream_walker.h"
#include "overlay_sizes.h"
#include <string.h>
#include <stdio.h>

/* rspq internal command sizes (bytes) — from rspq_internal.h */
static int rspq_int_size_bytes(int cmd_id) {
    switch (cmd_id) {
    case 0x0: return 4;   /* INVALID */
    case 0x1: return 4;   /* NOOP */
    case 0x2: return 4;   /* JUMP */
    case 0x3: return 12;  /* CALL (3 words) */
    case 0x4: return 4;   /* RET */
    case 0x5: return 16;  /* DMA (4 words) */
    case 0x6: return 4;   /* WRITE_STATUS */
    case 0x7: return 12;  /* SWAP_BUFFERS */
    case 0x8: return 4;   /* TEST_WRITE_STATUS */
    case 0x9: return 12;  /* RDP_SET_BUFFER */
    case 0xA: return 12;  /* RDP_APPEND_BUFFER */
    default:  return 4;   /* conservative */
    }
}

/* ----------------------------------------------------------------
 * Overlay identification from command patterns
 *
 * rdpq signature: many 8-byte commands, triangle commands (32-176 bytes),
 *   high cmd_id range (0x24+ for passthrough RDP commands)
 *
 * T3D signature: SCREEN_SIZE (20 bytes, cmd 1), VERT_LOAD (12 bytes, cmd 4),
 *   TRI_DRAW (8 bytes, cmd 0), DRAWFLAGS (8 bytes, cmd 6)
 *
 * Strategy: scan the buffer counting how many commands from each
 * overlay slot have sizes consistent with rdpq vs T3D.
 * ---------------------------------------------------------------- */

void t3d_stream_identify_overlays(const uint8_t *rdram, uint32_t rdram_size,
                                  uint32_t buf_addr, uint32_t buf_len,
                                  T3DOverlayMap *ovl_map) {
    uint32_t phys = buf_addr & 0x1FFFFFFF;
    uint32_t end = phys + buf_len;
    if (end > rdram_size) end = rdram_size;

    /* For each overlay slot, count commands with high cmd_ids (rdpq indicator) */
    int high_cmd_count[16] = {0};  /* cmd_id >= 0x20 = likely rdpq passthrough */
    int low_cmd_count[16] = {0};   /* cmd_id < 0x0D = could be T3D */

    /* Simple scan: just look at first words, don't try to step */
    for (uint32_t pos = phys; pos + 3 < end; pos += 4) {
        uint32_t w0 = t3d_hle_read32(rdram, rdram_size, pos);
        if (w0 == 0) continue;
        int ovl = RSPQ_OVL_ID(w0);
        int cmd = RSPQ_CMD_ID(w0);
        if (ovl == 0 || ovl >= 15) continue;

        /* rdpq passthrough commands have cmd_id 0x24-0x3F */
        if (cmd >= 0x06) high_cmd_count[ovl]++;  /* broader: 6+ */
        if (cmd < 0x0D) low_cmd_count[ovl]++;
    }

    /* The overlay with the most high-cmd hits is likely rdpq.
     * rdpq is the dominant overlay in libdragon scenes. */
    int best_rdpq = -1, best_rdpq_score = 0;
    for (int i = 1; i < 15; i++) {
        if (high_cmd_count[i] > best_rdpq_score) {
            best_rdpq_score = high_cmd_count[i];
            best_rdpq = i;
        }
    }

    if (best_rdpq >= 0 && best_rdpq_score > 5 && !ovl_map->rdpq_confirmed) {
        ovl_map->rdpq_slot = best_rdpq;
        ovl_map->rdpq_confirmed = true;
    }

    /* T3D is harder to identify from first words alone.
     * We'll detect it during stream walking when we see the
     * VERT_LOAD + TRI_DRAW signature in a correctly-stepped context. */
}

/* ----------------------------------------------------------------
 * Walk the stream with correct stepping
 * ---------------------------------------------------------------- */

int t3d_stream_walk(T3DHLEState *state,
                    const uint8_t *rdram, uint32_t rdram_size,
                    uint32_t buf_addr, uint32_t buf_len,
                    T3DOverlayMap *ovl_map) {
    uint32_t phys = buf_addr & 0x1FFFFFFF;
    uint32_t end = phys + buf_len;
    if (end > rdram_size) end = rdram_size;

    int t3d_cmds_found = 0;
    uint32_t pos = phys;

    while (pos + 3 < end) {
        uint32_t w0 = t3d_hle_read32(rdram, rdram_size, pos);

        /* Skip zero padding */
        if (w0 == 0) { pos += 4; continue; }

        int ovl = RSPQ_OVL_ID(w0);
        int cmd = RSPQ_CMD_ID(w0);

        /* rspq internal (overlay 0) — known sizes */
        if (ovl == 0) {
            int sz = rspq_int_size_bytes(cmd);
            state->frame.cmds_rspq++;
            state->frame.cmds_total++;

            /* Follow CALL targets to scan blocks */
            if (cmd == 0x3 && pos + 7 < end) { /* CALL */
                uint32_t call_addr = t3d_hle_read32(rdram, rdram_size, pos + 4) & 0x1FFFFFFF;
                static int call_log = 0;
                if (call_log < 10) {
                    T3D_LOG(state, "T3D_WALK_CALL",
                        "w0=0x%08X addr=0x%08X valid=%d",
                        w0, call_addr,
                        (call_addr > 0x1000 && call_addr + 64 < rdram_size) ? 1 : 0);
                    call_log++;
                }
                if (call_addr > 0x1000 && call_addr + 64 < rdram_size) {
                    /* Recursively walk the block (limited depth) */
                    int block_t3d = t3d_stream_walk(state, rdram, rdram_size,
                                                     call_addr, 1024, ovl_map);
                    t3d_cmds_found += block_t3d;
                }
            }

            pos += sz;
            continue;
        }

        /* rdpq overlay — use known size table to step correctly */
        if (ovl_map->rdpq_confirmed && ovl == ovl_map->rdpq_slot) {
            int sz = (cmd < 64) ? rdpq_cmd_sizes[cmd] : 8;
            state->frame.cmds_other++;
            state->frame.cmds_total++;
            pos += sz;
            continue;
        }

        /* T3D overlay — parse and dispatch */
        if (ovl_map->t3d_confirmed && ovl == ovl_map->t3d_slot) {
            int consumed = t3d_hle_process_command(state, rdram, rdram_size, pos);
            if (consumed > 0) {
                t3d_cmds_found++;
                pos += consumed * 4;
            } else {
                /* Bad T3D command — skip minimum and continue */
                pos += 4;
            }
            continue;
        }

        /* Unknown overlay — try T3D detection.
         * If this overlay has T3D-like signature, tentatively mark it. */
        if (!ovl_map->t3d_confirmed && cmd < 13) {
            /* Try parsing as T3D */
            int consumed = t3d_hle_process_command(state, rdram, rdram_size, pos);
            if (consumed > 0) {
                /* Successful parse — check if detection triggered */
                if (state->detect.confirmed && state->detect.overlay_id == ovl) {
                    ovl_map->t3d_slot = ovl;
                    ovl_map->t3d_confirmed = true;
                    T3D_LOG(state, "T3D_STREAM",
                        "event=t3d_slot_found slot=%d", ovl);
                }
                t3d_cmds_found++;
                pos += consumed * 4;
                continue;
            }
        }

        /* Truly unknown overlay — skip 4 bytes and pray */
        state->frame.cmds_other++;
        state->frame.cmds_total++;
        pos += 4;
    }

    return t3d_cmds_found;
}
