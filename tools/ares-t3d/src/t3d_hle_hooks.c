/*
 * t3d_hle_hooks.c — ares hook entry points
 *
 * Manages the global T3DHLEState singleton and routes ares events
 * to the HLE parser via the correct mixed-stream walker.
 */

#include "t3d_hle.h"
#include "t3d_hle_hooks.h"
#include "stream_walker.h"
#include "cpu_intercept.h"
#include <string.h>
#include <stdio.h>

/* ----------------------------------------------------------------
 * Global singleton
 * ---------------------------------------------------------------- */
static T3DHLEState g_hle;
static T3DOverlayMap g_ovl_map;
static T3DCpuIntercept g_cpu;
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
    memset(&g_ovl_map, 0, sizeof(g_ovl_map));
    g_ovl_map.rdpq_slot = -1;
    g_ovl_map.t3d_slot = -1;
    t3d_cpu_init(&g_cpu);
    g_initialized = true;
    T3D_LOG(&g_hle, "T3D_HOOK", "event=system_init rdram_size=%u", rdram_size);
}

/* ----------------------------------------------------------------
 * on_rsp_dma — called after every RSP DMA transfer
 *
 * Uses the correct mixed-stream walker with rdpq command size table
 * to step through multiplexed command buffers without misalignment.
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

    /* Notify CPU intercept of DMA for buffer address discovery */
    t3d_cpu_on_dma_to_dmem(&g_cpu, phys_dram, pbus_addr, length);

    /* DMA to DMEM — stream walker for overlay identification */
    if (pbus_addr >= 0x200 || length > 512 || length < 8) return;

    /* Identify rdpq overlay slot on early buffers */
    if (!g_ovl_map.rdpq_confirmed) {
        t3d_stream_identify_overlays(rdram, rdram_size,
                                     phys_dram, length, &g_ovl_map);
        if (g_ovl_map.rdpq_confirmed) {
            T3D_LOG(&g_hle, "T3D_STREAM",
                "event=rdpq_slot_found slot=%d", g_ovl_map.rdpq_slot);
        }
    }

    /* Walk the buffer with correct command stepping */
    int t3d_found = t3d_stream_walk(&g_hle, rdram, rdram_size,
                                     phys_dram, length, &g_ovl_map);

    if (t3d_found > 0 && g_hle.verbose) {
        T3D_LOG(&g_hle, "T3D_DMA", "target=DMEM dram=0x%08X len=%u t3d=%d rdpq=%d t3d_slot=%d",
                phys_dram, length, t3d_found, g_ovl_map.rdpq_slot, g_ovl_map.t3d_slot);
    }
}

/* ----------------------------------------------------------------
 * on_sp_status — CPU writes to SP_STATUS register
 * ---------------------------------------------------------------- */

void t3d_hle_on_sp_status(uint32_t data) {
    if (!g_initialized || !g_rdram) return;

    /* Sync overlay knowledge from stream walker to cpu intercept */
    if (g_ovl_map.rdpq_confirmed && g_cpu.rdpq_slot < 0) {
        g_cpu.rdpq_slot = g_ovl_map.rdpq_slot;
    }
    if (g_ovl_map.t3d_confirmed && g_cpu.t3d_slot < 0) {
        g_cpu.t3d_slot = g_ovl_map.t3d_slot;
    }

    t3d_cpu_on_sp_status_write(&g_cpu, &g_hle, data, g_rdram, g_rdram_size);
}

/* ----------------------------------------------------------------
 * on_vblank — frame boundary
 * ---------------------------------------------------------------- */

void t3d_hle_on_vblank(void) {
    if (!g_initialized) return;

    /* Log overlay map state periodically */
    if ((g_hle.total_frames % 60) == 0 && g_hle.total_frames > 0) {
        T3D_LOG(&g_hle, "T3D_OVL_MAP",
            "rdpq=%d(conf=%d) t3d=%d(conf=%d)",
            g_ovl_map.rdpq_slot, g_ovl_map.rdpq_confirmed,
            g_ovl_map.t3d_slot, g_ovl_map.t3d_confirmed);
    }

    t3d_hle_frame_end(&g_hle);
}
