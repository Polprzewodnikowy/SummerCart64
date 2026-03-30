/*
 * t3d_hle_hooks.c — ares hook entry points
 *
 * Manages the global T3DHLEState singleton and routes ares events
 * to the HLE parser. This is the bridge between ares and t3d_hle.c.
 */

#include "t3d_hle.h"
#include "t3d_hle_hooks.h"
#include <string.h>
#include <stdio.h>

/* ----------------------------------------------------------------
 * Global singleton
 * ---------------------------------------------------------------- */
static T3DHLEState g_hle;
static const uint8_t *g_rdram = NULL;
static uint32_t g_rdram_size = 0;
static bool g_initialized = false;

/* ----------------------------------------------------------------
 * system_init — called once at startup
 * ---------------------------------------------------------------- */

void t3d_hle_system_init(uint8_t *rdram_data, uint32_t rdram_size) {
    g_rdram = rdram_data;
    g_rdram_size = rdram_size;
    t3d_hle_init(&g_hle, 0);
    g_initialized = true;
    T3D_LOG(&g_hle, "T3D_HOOK", "event=system_init rdram_size=%u", rdram_size);
}

/* ----------------------------------------------------------------
 * Validate if a 32-bit word looks like a plausible rspq command.
 * Heuristic: overlay ID 0-15, and if overlay 0, cmd should be 0-7.
 * ---------------------------------------------------------------- */
static bool looks_like_rspq_cmd(uint32_t w0) {
    if (w0 == 0) return false; /* padding */
    int ovl = RSPQ_OVL_ID(w0);
    int cmd = RSPQ_CMD_ID(w0);
    /* overlay 0: internal commands 0-7 */
    if (ovl == 0 && cmd > 7) return false;
    /* overlay 15 is unlikely (rspq supports 0-14 in practice) */
    if (ovl == 15) return false;
    return true;
}

/* ----------------------------------------------------------------
 * Estimate command size for a non-T3D overlay command.
 * For rspq internal (overlay 0), we know exact sizes.
 * For unknown overlays, we can't know — return 0 to signal
 * "stop scanning" rather than guess and misalign.
 * ---------------------------------------------------------------- */
static int rspq_internal_cmd_size(int cmd_id) {
    switch (cmd_id) {
    case RSPQ_CMD_NOOP:    return 1;
    case RSPQ_CMD_JUMP:    return 1; /* addr in args */
    case RSPQ_CMD_CALL:    return 2; /* addr + return */
    case RSPQ_CMD_RET:     return 1;
    case RSPQ_CMD_DMA:     return 4; /* rdram + dmem + length + ... */
    case RSPQ_CMD_WRITE:   return 2;
    case RSPQ_CMD_TEST:    return 1;
    case RSPQ_CMD_WSTATUS: return 1;
    default: return 1;
    }
}

/* ----------------------------------------------------------------
 * on_rsp_dma — called after every RSP DMA transfer
 * ---------------------------------------------------------------- */

void t3d_hle_on_rsp_dma(uint32_t dram_addr, uint32_t pbus_addr,
                         uint32_t length, uint32_t region,
                         const uint8_t *rdram, uint32_t rdram_size,
                         const uint8_t *region_data) {
    if (!g_initialized) return;
    (void)region_data;

    g_rdram = rdram;
    g_rdram_size = rdram_size;

    uint32_t phys_dram = dram_addr & 0x1FFFFFFF;

    if (region == 1) {
        /* DMA to IMEM — microcode load */
        if (g_hle.verbose) {
            T3D_LOG(&g_hle, "T3D_DMA", "target=IMEM dram=0x%08X pbus=0x%04X len=%u",
                    phys_dram, pbus_addr, length);
        }
        return;
    }

    /* ---- DMA Survey: record ALL DMEM DMAs before filtering ---- */
    if (g_hle.dma_survey_enabled && length >= 4) {
        /* Find or create bucket for this pbus range (quantize to 256-byte blocks) */
        uint32_t bucket_key = pbus_addr / 256;
        T3DDMASurveyBucket *bkt = NULL;
        for (int i = 0; i < g_hle.dma_survey_count; i++) {
            if (g_hle.dma_survey[i].pbus_lo / 256 == bucket_key) {
                bkt = &g_hle.dma_survey[i];
                break;
            }
        }
        if (!bkt && g_hle.dma_survey_count < T3D_DMA_SURVEY_BUCKETS) {
            bkt = &g_hle.dma_survey[g_hle.dma_survey_count++];
            memset(bkt, 0, sizeof(*bkt));
            bkt->pbus_lo = pbus_addr;
            bkt->pbus_hi = pbus_addr;
        }
        if (bkt) {
            bkt->count++;
            bkt->total_bytes += length;
            if (pbus_addr < bkt->pbus_lo) bkt->pbus_lo = pbus_addr;
            if (pbus_addr > bkt->pbus_hi) bkt->pbus_hi = pbus_addr;

            /* Classify first word */
            uint32_t w0 = t3d_hle_read32(rdram, rdram_size, phys_dram);
            int ovl = RSPQ_OVL_ID(w0);
            if (w0 == 0) bkt->has_zero++;
            else if (ovl == 0) bkt->has_rspq_cmds++;
            else if (g_hle.detect.overlay_id >= 0 && ovl == g_hle.detect.overlay_id)
                bkt->has_t3d_cmds++;
            else bkt->has_other_cmds++;
        }
    }

    /* DMA to DMEM — only scan if destination is in the rspq prefetch
     * region (low DMEM) and length is command-buffer-sized. */
    if (pbus_addr >= 0x200 || length > 512 || length < 8) return;

    /* Validate first word as a plausible rspq command */
    if (phys_dram + 3 >= rdram_size) return;
    uint32_t first_word = t3d_hle_read32(rdram, rdram_size, phys_dram);
    if (!looks_like_rspq_cmd(first_word)) return;

    /* Scan the buffer for commands */
    uint32_t scan_end = phys_dram + length;
    if (scan_end > rdram_size) scan_end = rdram_size;

    uint32_t pos = phys_dram;
    int cmds_in_buffer = 0;

    while (pos + 3 < scan_end) {
        uint32_t w0 = t3d_hle_read32(rdram, rdram_size, pos);

        if (w0 == 0) { pos += 4; continue; }
        if (!looks_like_rspq_cmd(w0)) break; /* garbage — stop */

        int ovl_id = RSPQ_OVL_ID(w0);
        int cmd_id = RSPQ_CMD_ID(w0);

        /* rspq internal commands (overlay 0) */
        if (ovl_id == 0) {
            int size = rspq_internal_cmd_size(cmd_id);
            g_hle.frame.cmds_rspq++;
            g_hle.frame.cmds_total++;
            pos += size * 4;
            cmds_in_buffer++;
            continue;
        }

        /* T3D overlay — parse fully */
        if (g_hle.detect.overlay_id == ovl_id ||
            (g_hle.detect.overlay_id == -1 && cmd_id < T3D_CMD_COUNT)) {
            int consumed = t3d_hle_process_command(&g_hle, rdram, rdram_size, pos);
            if (consumed > 0) {
                pos += consumed * 4;
                cmds_in_buffer++;
                continue;
            }
            /* consumed <= 0 means unknown/bad command — stop scanning */
            break;
        }

        /* Unknown overlay command — we don't know its exact size.
         * Strategy: skip forward looking for the next recognizable
         * command word (T3D overlay or rspq overlay 0).
         * Limit scan to 16 words to avoid runaway. */
        g_hle.frame.cmds_other++;
        g_hle.frame.cmds_total++;
        {
            bool found_next = false;
            uint32_t skip_pos = pos + 4; /* skip at least 1 word */
            int skip_limit = 16;
            while (skip_pos + 3 < scan_end && skip_limit-- > 0) {
                uint32_t probe = t3d_hle_read32(rdram, rdram_size, skip_pos);
                if (probe == 0) { skip_pos += 4; continue; }
                int probe_ovl = RSPQ_OVL_ID(probe);
                int probe_cmd = RSPQ_CMD_ID(probe);
                /* Recognizable: overlay 0 with valid cmd, or our T3D overlay */
                if (probe_ovl == 0 && probe_cmd <= 7) { found_next = true; break; }
                if (g_hle.detect.overlay_id >= 0 && probe_ovl == g_hle.detect.overlay_id
                    && probe_cmd < T3D_CMD_COUNT) { found_next = true; break; }
                skip_pos += 4;
            }
            if (found_next) {
                pos = skip_pos;
                cmds_in_buffer++;
                continue;
            }
            break; /* couldn't find next valid command — stop */
        }
    }

    if (cmds_in_buffer > 0 && g_hle.verbose) {
        T3D_LOG(&g_hle, "T3D_DMA", "target=DMEM dram=0x%08X len=%u cmds=%d",
                phys_dram, length, cmds_in_buffer);
    }
}

/* ----------------------------------------------------------------
 * on_vblank — frame boundary
 * ---------------------------------------------------------------- */

void t3d_hle_on_vblank(void) {
    if (!g_initialized) return;
    t3d_hle_frame_end(&g_hle);
}
