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
 * RDRAM write watchpoint — captures rspq buffer writes at emission
 *
 * The watched range is discovered from DMA patterns.
 * Writes are accumulated into a segment buffer. When SP_STATUS
 * SIG_MORE fires, the segment is flushed and decoded.
 * ---------------------------------------------------------------- */

/* Write capture buffer: accumulates contiguous writes */
#define WCAP_MAX_WORDS 512
static struct {
    uint32_t base_addr;         /* RDRAM start of current segment */
    uint32_t words[WCAP_MAX_WORDS];
    int count;                  /* words accumulated */
    uint32_t last_addr;         /* last write address */

    /* Watched address ranges (from DMA discovery) */
    uint32_t watch_lo;          /* start of watched range */
    uint32_t watch_hi;          /* end of watched range */
    bool watch_active;

    /* Stats */
    uint32_t writes_total;
    uint32_t segments_flushed;
} g_wcap;

static void wcap_flush(void) {
    if (g_wcap.count == 0) return;

    g_wcap.segments_flushed++;

    /* Log the segment */
    static int seg_log = 0;
    if (seg_log < 50) {
        T3D_LOG(&g_hle, "T3D_WCAP_SEG",
            "addr=0x%08X words=%d seg=%u",
            g_wcap.base_addr, g_wcap.count, g_wcap.segments_flushed);

        /* Decode first few words */
        if (g_wcap.count >= 1) {
            uint32_t w0 = g_wcap.words[0];
            int ovl = RSPQ_OVL_ID(w0);
            int cmd = RSPQ_CMD_ID(w0);
            T3D_LOG(&g_hle, "T3D_WCAP_FIRST",
                "w0=0x%08X ovl=%d cmd=%d name=%s",
                w0, ovl, cmd,
                (ovl == g_ovl_map.t3d_slot && cmd < T3D_CMD_COUNT)
                    ? t3d_hle_cmd_name(cmd) : "?");
        }
        seg_log++;
    }

    /* Scan for CALL commands (overlay 0, cmd 3) in captured words.
     * CALL format: [0x03XXXXXX] [block_addr] [nesting]
     * Extract block addresses and scan them directly in real RDRAM. */
    for (int i = 0; i < g_wcap.count - 2; i++) {
        uint32_t w = g_wcap.words[i];
        if ((w >> 28) == 0 && ((w >> 24) & 0xF) == 0x3) {
            uint32_t block_addr = g_wcap.words[i + 1] & 0x1FFFFFFF;
            static int call_extract_log = 0;
            if (call_extract_log < 30) {
                T3D_LOG(&g_hle, "T3D_WCAP_CALL",
                    "w0=0x%08X block=0x%08X nesting=0x%08X",
                    w, block_addr, g_wcap.words[i + 2]);
                call_extract_log++;

                /* If the block address is in heap range, scan it! */
                if (g_rdram && block_addr > 0x10000 && block_addr + 64 < g_rdram_size) {
                    /* Read first 8 words of block from real RDRAM */
                    T3D_LOG(&g_hle, "T3D_WCAP_BLOCK",
                        "addr=0x%08X first=[%08X %08X %08X %08X]",
                        block_addr,
                        t3d_hle_read32(g_rdram, g_rdram_size, block_addr),
                        t3d_hle_read32(g_rdram, g_rdram_size, block_addr + 4),
                        t3d_hle_read32(g_rdram, g_rdram_size, block_addr + 8),
                        t3d_hle_read32(g_rdram, g_rdram_size, block_addr + 12));
                }
            }
        }
    }

    /* Build a fake RDRAM-like buffer for the parser.
     * The words are in host byte order (already swapped by ares).
     * We need to convert back to big-endian for our parser. */
    uint8_t buf[WCAP_MAX_WORDS * 4];
    for (int i = 0; i < g_wcap.count && i < WCAP_MAX_WORDS; i++) {
        uint32_t v = g_wcap.words[i];
        buf[i*4+0] = (v >> 24) & 0xFF;
        buf[i*4+1] = (v >> 16) & 0xFF;
        buf[i*4+2] = (v >>  8) & 0xFF;
        buf[i*4+3] = (v >>  0) & 0xFF;
    }

    /* Parse with the stream walker */
    if (g_rdram && g_rdram_size > 0) {
        /* Use the real RDRAM for CALL following etc, but parse
         * the captured segment directly */
        T3DOverlayMap tmp_map = g_ovl_map;
        t3d_stream_walk(&g_hle, buf, g_wcap.count * 4,
                        0, g_wcap.count * 4, &tmp_map);
    }

    g_wcap.count = 0;
}

void t3d_hle_on_rdram_write(uint32_t address, uint32_t value) {
    if (!g_initialized) return;

    /* Address-based filter with auto-discovery from DMA.
     * Watch the first discovered buffer range (~2KB). */
    if (!g_wcap.watch_active) {
        /* Use the 0x0057XXXX range directly — this is where the most
         * rspq DMA activity occurs, meaning it's the active command buffer.
         * We hardcode this for now; a proper discovery would track
         * the most-frequently-DMA'd range. */
        if (g_hle.total_frames > 70) { /* wait for game to start */
            g_wcap.watch_lo = 0x0057D000;
            g_wcap.watch_hi = 0x00580000; /* 12KB range */
            g_wcap.watch_active = true;
            T3D_LOG(&g_hle, "T3D_WCAP",
                "event=watch_armed lo=0x%08X hi=0x%08X size=%u",
                g_wcap.watch_lo, g_wcap.watch_hi,
                g_wcap.watch_hi - g_wcap.watch_lo);
        }
        return;
    }

    /* Fast range check */
    if (address < g_wcap.watch_lo || address >= g_wcap.watch_hi) return;

    g_wcap.writes_total++;

    /* Check for contiguous writes */
    if (g_wcap.count > 0 && address != g_wcap.last_addr + 4) {
        /* Gap in addresses — flush current segment */
        wcap_flush();
    }

    /* Start new segment if empty */
    if (g_wcap.count == 0) {
        g_wcap.base_addr = address;
    }

    /* Accumulate */
    if (g_wcap.count < WCAP_MAX_WORDS) {
        g_wcap.words[g_wcap.count++] = value;
    }
    g_wcap.last_addr = address;

    /* Auto-flush at buffer boundaries or max size */
    if (g_wcap.count >= WCAP_MAX_WORDS) {
        wcap_flush();
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

    /* Flush write capture buffer on SIG_MORE (publish boundary) */
    if (data & (1 << 24)) { /* bit 24 = set signal 7 (SIG_MORE) */
        wcap_flush();
    }

    t3d_cpu_on_sp_status_write(&g_cpu, &g_hle, data, g_rdram, g_rdram_size);
}

/* ----------------------------------------------------------------
 * on_vblank — frame boundary
 * ---------------------------------------------------------------- */

void t3d_hle_on_vblank(void) {
    if (!g_initialized) return;

    /* One-time full RDRAM scan for VERT_LOAD patterns in blocks.
     * Run once after the game has loaded (frame 80). */
    static bool rdram_scanned = false;
    if (!rdram_scanned && g_hle.total_frames == 80 && g_rdram && g_rdram_size > 0) {
        rdram_scanned = true;
        T3D_LOG(&g_hle, "T3D_SCAN", "event=full_rdram_scan size=%u", g_rdram_size);

        int t3d_ovl = g_ovl_map.t3d_confirmed ? g_ovl_map.t3d_slot : 1;
        int hits = 0;

        for (uint32_t addr = 0x10000; addr + 11 < g_rdram_size; addr += 4) {
            uint32_t w0 = t3d_hle_read32(g_rdram, g_rdram_size, addr);
            int ovl = RSPQ_OVL_ID(w0);
            int cmd = RSPQ_CMD_ID(w0);

            /* Look for ANY overlay with cmd=4 (VERT_LOAD-like) */
            if (cmd == T3D_CMD_VERT_LOAD && ovl > 0 && ovl < 15) {
                uint32_t input_size = RSPQ_ARGS(w0);
                uint32_t w1 = t3d_hle_read32(g_rdram, g_rdram_size, addr + 4);
                uint32_t phys = w1 & 0x1FFFFFFF;

                /* Check plausibility */
                if (input_size > 0 && input_size <= 1120 &&
                    (input_size % 16) == 0 &&
                    phys > 0x10000 && phys < g_rdram_size) {
                    hits++;

                    /* Use the FIRST hit to set the T3D overlay slot.
                     * This is ground truth — VERT_LOAD with sane args
                     * can only come from the real T3D overlay. */
                    if (hits == 1) {
                        g_ovl_map.t3d_slot = ovl;
                        g_ovl_map.t3d_confirmed = true;
                        g_hle.detect.overlay_id = ovl;
                        g_hle.detect.confirmed = true;
                        g_hle.detect.confidence = 99;
                        g_cpu.t3d_slot = ovl;
                        T3D_LOG(&g_hle, "T3D_SCAN",
                            "event=t3d_overlay_found slot=%d via=VERT_LOAD_scan", ovl);
                    }

                    if (hits <= 20) {
                        uint32_t w2 = t3d_hle_read32(g_rdram, g_rdram_size, addr + 8);
                        T3D_LOG(&g_hle, "T3D_SCAN_HIT",
                            "addr=0x%08X w0=0x%08X w1=0x%08X w2=0x%08X "
                            "ovl=%d verts=%u rdram_addr=0x%08X",
                            addr, w0, w1, w2,
                            ovl, input_size / 16, phys);
                    }
                }
            }
        }
        T3D_LOG(&g_hle, "T3D_SCAN", "event=scan_complete hits=%d t3d_ovl=%d", hits,
                g_ovl_map.t3d_slot);

        /* Now parse each block region around the VERT_LOAD hits.
         * Scan backwards to find the block start (DRAWFLAGS or start of
         * contiguous T3D commands), then forward to TRI_SYNC/RET. */
        if (hits > 0 && g_ovl_map.t3d_confirmed) {
            int t3d_ovl_id = g_ovl_map.t3d_slot;
            T3D_LOG(&g_hle, "T3D_BLOCK_PARSE", "event=start parsing %d blocks", hits);

            /* Re-scan to find and parse each block */
            for (uint32_t a = 0x10000; a + 11 < g_rdram_size; a += 4) {
                uint32_t w = t3d_hle_read32(g_rdram, g_rdram_size, a);
                int o = RSPQ_OVL_ID(w);
                int c = RSPQ_CMD_ID(w);
                if (o != t3d_ovl_id || c != T3D_CMD_VERT_LOAD) continue;

                uint32_t isz = RSPQ_ARGS(w);
                uint32_t w1v = t3d_hle_read32(g_rdram, g_rdram_size, a + 4);
                uint32_t pv = w1v & 0x1FFFFFFF;
                if (isz == 0 || isz > 1120 || (isz % 16) != 0) continue;
                if (pv < 0x10000 || pv >= g_rdram_size) continue;

                /* Found a valid VERT_LOAD. Scan backwards for block start. */
                uint32_t block_start = a;
                for (int back = 1; back < 20; back++) {
                    uint32_t prev = a - back * 4;
                    if (prev < 0x10000) break;
                    uint32_t pw = t3d_hle_read32(g_rdram, g_rdram_size, prev);
                    int po = RSPQ_OVL_ID(pw);
                    if (po == t3d_ovl_id || po == 0) {
                        block_start = prev;
                    } else {
                        break; /* hit non-T3D/non-rspq word */
                    }
                }

                /* Parse from block_start forward with the T3D parser */
                T3D_LOG(&g_hle, "T3D_BLOCK_PARSE",
                    "block_start=0x%08X vert_load_at=0x%08X",
                    block_start, a);

                uint32_t bpos = block_start;
                uint32_t bend = a + 512; /* max 512 bytes past VERT_LOAD */
                if (bend > g_rdram_size) bend = g_rdram_size;
                int block_cmds = 0;

                while (bpos + 3 < bend && block_cmds < 64) {
                    uint32_t bw = t3d_hle_read32(g_rdram, g_rdram_size, bpos);
                    if (bw == 0) { bpos += 4; continue; }
                    int bo = RSPQ_OVL_ID(bw);
                    int bc = RSPQ_CMD_ID(bw);

                    if (bo == 0 && bc == RSPQ_CMD_RET) break; /* end of block */

                    if (bo == t3d_ovl_id && bc < T3D_CMD_COUNT) {
                        int consumed = t3d_hle_process_command(
                            &g_hle, g_rdram, g_rdram_size, bpos);
                        if (consumed > 0) {
                            bpos += consumed * 4;
                            block_cmds++;
                            continue;
                        }
                    }
                    bpos += 4; /* skip unknown */
                    block_cmds++;
                }

                T3D_LOG(&g_hle, "T3D_BLOCK_PARSE",
                    "block done: %d cmds, verts=%d tris=%d",
                    block_cmds, g_hle.frame.verts_loaded, g_hle.frame.tris_drawn);
            }
        }
    }

    /* Log overlay map state periodically */
    if ((g_hle.total_frames % 60) == 0 && g_hle.total_frames > 0) {
        T3D_LOG(&g_hle, "T3D_OVL_MAP",
            "rdpq=%d(conf=%d) t3d=%d(conf=%d)",
            g_ovl_map.rdpq_slot, g_ovl_map.rdpq_confirmed,
            g_ovl_map.t3d_slot, g_ovl_map.t3d_confirmed);
    }

    t3d_hle_frame_end(&g_hle);
}
