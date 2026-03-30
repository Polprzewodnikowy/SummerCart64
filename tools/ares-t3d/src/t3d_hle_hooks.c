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
 * rspq DMEM layout constants (from libdragon rspq_constants.h)
 *
 * The rspq engine stores its read pointer and other state in
 * fixed DMEM offsets. We observe these after DMA to detect when
 * a new command buffer has been loaded.
 * ---------------------------------------------------------------- */

/* rspq uses a double-buffered DMEM region for command prefetch.
 * The exact layout varies by libdragon version, but the command
 * data typically occupies DMEM 0x100-0x1FF (256 bytes).
 *
 * For Tier 1 telemetry, we don't intercept at the DMEM level.
 * Instead, we scan RDRAM command buffers directly. The RSP DMA
 * to DMEM tells us WHICH RDRAM region contains commands. */

/* ----------------------------------------------------------------
 * system_init — called once at startup
 * ---------------------------------------------------------------- */

void t3d_hle_system_init(uint8_t *rdram_data, uint32_t rdram_size) {
    g_rdram = rdram_data;
    g_rdram_size = rdram_size;

    /* Initialize in telemetry mode (Tier 1) */
    t3d_hle_init(&g_hle, 0);
    g_initialized = true;

    T3D_LOG(&g_hle, "T3D_HOOK", "event=system_init rdram_size=%u", rdram_size);
}

/* ----------------------------------------------------------------
 * on_rsp_dma — called after every RSP DMA transfer
 *
 * This is the primary ingestion point. We observe:
 * - DMA to IMEM (region=1): ucode loading → overlay detection
 * - DMA to DMEM (region=0): command/data buffers → parse commands
 *
 * For DMEM DMA, the source RDRAM region often contains rspq
 * command buffers. We scan them for T3D commands.
 * ---------------------------------------------------------------- */

void t3d_hle_on_rsp_dma(uint32_t dram_addr, uint32_t pbus_addr,
                         uint32_t length, uint32_t region,
                         const uint8_t *rdram, uint32_t rdram_size,
                         const uint8_t *region_data) {
    if (!g_initialized) return;

    /* Update RDRAM pointer (may change on system reset) */
    g_rdram = rdram;
    g_rdram_size = rdram_size;

    uint32_t phys_dram = dram_addr & 0x1FFFFFFF;

    if (region == 1) {
        /* DMA to IMEM — potential microcode load.
         * Log it for overlay detection (Tier 1 telemetry). */
        if (g_hle.verbose) {
            T3D_LOG(&g_hle, "T3D_DMA", "target=IMEM dram=0x%08X pbus=0x%04X len=%u",
                phys_dram, pbus_addr, length);
        }
        /* TODO: IMEM fingerprinting for rspq/T3D overlay detection.
         * For now, detection happens via command pattern matching. */
        return;
    }

    /* DMA to DMEM — could be command buffer, vertex data, or overlay table.
     *
     * rspq command buffers are loaded into DMEM at offset 0x000-0x0FF
     * (the prefetch buffer). The source RDRAM address tells us where
     * the command buffer lives. We scan that RDRAM region directly.
     *
     * Filter: only scan small DMAs to low DMEM addresses (likely commands).
     * Large DMAs to high DMEM are usually data (vertex buffers loaded by
     * the RSP itself, not rspq command data). */
    if (pbus_addr < 0x200 && length <= 512) {
        /* This looks like a command buffer prefetch. Scan the source
         * RDRAM region for rspq commands. */
        uint32_t scan_end = phys_dram + length;
        if (scan_end > rdram_size) scan_end = rdram_size;

        uint32_t pos = phys_dram;
        int cmds_in_buffer = 0;

        while (pos + 3 < scan_end) {
            uint32_t w0 = t3d_hle_read32(rdram, rdram_size, pos);

            /* Skip zero words (padding/empty slots) */
            if (w0 == 0) { pos += 4; continue; }

            int ovl_id = RSPQ_OVL_ID(w0);

            /* rspq internal commands (overlay 0) — skip but count */
            if (ovl_id == 0) {
                int cmd = RSPQ_CMD_ID(w0);
                /* JUMP/CALL/RET are 1-2 words. Be conservative. */
                int size = (cmd == RSPQ_CMD_JUMP || cmd == RSPQ_CMD_CALL) ? 2 : 1;
                g_hle.frame.cmds_rspq++;
                g_hle.frame.cmds_total++;
                pos += size * 4;
                cmds_in_buffer++;
                continue;
            }

            /* Non-zero overlay — try our T3D parser */
            int consumed = t3d_hle_process_command(&g_hle, rdram, rdram_size, pos);
            if (consumed > 0) {
                pos += consumed * 4;
                cmds_in_buffer++;
            } else {
                /* Unknown command from unknown overlay. Advance 1 word
                 * to avoid infinite loop, but this is a heuristic. */
                pos += 4;
                cmds_in_buffer++;
            }

            /* Safety: don't scan more than 64 commands per buffer */
            if (cmds_in_buffer > 64) break;
        }

        if (cmds_in_buffer > 0 && g_hle.verbose) {
            T3D_LOG(&g_hle, "T3D_DMA", "target=DMEM dram=0x%08X len=%u cmds=%d",
                phys_dram, length, cmds_in_buffer);
        }
    }
}

/* ----------------------------------------------------------------
 * on_vblank — frame boundary
 * ---------------------------------------------------------------- */

void t3d_hle_on_vblank(void) {
    if (!g_initialized) return;
    t3d_hle_frame_end(&g_hle);
}
