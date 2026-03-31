/*
 * t3d_hle_hooks.c — ares hook entry points (definitive version)
 *
 * Primary approach: RDRAM block scan at frame 80.
 * Scans all RDRAM for T3D VERT_LOAD patterns to find rspq_blocks,
 * then parses the blocks directly from static heap memory.
 */

#include "t3d_hle.h"
#include "t3d_hle_hooks.h"
#include "overlay_sizes.h"
#include <string.h>
#include <stdio.h>

static T3DHLEState g_hle;
static const uint8_t *g_rdram = NULL;
static uint32_t g_rdram_size = 0;
static bool g_initialized = false;
static int g_t3d_slot = -1;
static int g_rdpq_slot = -1;

#define MAX_BLOCKS 32
static struct {
    uint32_t vl_addr;
    uint32_t block_start;
    uint32_t vert_count;
    uint32_t vert_rdram;
    uint16_t dest_off;
    uint16_t input_off;
    int tri_count;
    int cmd_count;
} g_blocks[MAX_BLOCKS];
static int g_block_count = 0;

void t3d_hle_system_init(uint8_t *rdram_data, uint32_t rdram_size) {
    g_rdram = rdram_data;
    g_rdram_size = rdram_size;
    t3d_hle_init(&g_hle, 0);
    g_initialized = true;
    T3D_LOG(&g_hle, "T3D_HOOK", "event=system_init rdram_size=%u", rdram_size);
}

void t3d_hle_on_rsp_dma(uint32_t dram_addr, uint32_t pbus_addr,
                         uint32_t length, uint32_t region,
                         const uint8_t *rdram, uint32_t rdram_size,
                         const uint8_t *region_data) {
    if (!g_initialized) return;
    (void)region_data;
    g_rdram = rdram;
    g_rdram_size = rdram_size;

    if (region == 0 && pbus_addr < 0x200 && length >= 32 && g_rdpq_slot < 0) {
        uint32_t phys = dram_addr & 0x1FFFFFFF;
        int counts[16] = {0};
        for (uint32_t p = phys; p + 3 < phys + length && p + 3 < rdram_size; p += 4) {
            uint32_t w = t3d_hle_read32(rdram, rdram_size, p);
            if (w == 0) continue;
            int o = RSPQ_OVL_ID(w);
            if (o > 0 && o < 15) counts[o]++;
        }
        int best = -1, best_n = 5;
        for (int i = 1; i < 15; i++)
            if (counts[i] > best_n) { best_n = counts[i]; best = i; }
        if (best >= 0) {
            g_rdpq_slot = best;
            T3D_LOG(&g_hle, "T3D_DETECT", "event=rdpq_slot slot=%d", best);
        }
    }
}

void t3d_hle_on_sp_status(uint32_t data) { (void)data; }
void t3d_hle_on_rdram_write(uint32_t address, uint32_t value) { (void)address; (void)value; }

static void scan_rdram_for_blocks(void) {
    if (!g_rdram || g_rdram_size == 0) return;
    T3D_LOG(&g_hle, "T3D_SCAN", "event=start rdram_size=%u", g_rdram_size);
    g_block_count = 0;

    for (uint32_t addr = 0x10000; addr + 11 < g_rdram_size; addr += 4) {
        uint32_t w0 = t3d_hle_read32(g_rdram, g_rdram_size, addr);
        int ovl = RSPQ_OVL_ID(w0);
        int cmd = RSPQ_CMD_ID(w0);
        if (cmd != T3D_CMD_VERT_LOAD || ovl == 0 || ovl >= 15) continue;

        uint32_t isz = RSPQ_ARGS(w0);
        uint32_t w1 = t3d_hle_read32(g_rdram, g_rdram_size, addr + 4);
        uint32_t pa = w1 & 0x1FFFFFFF;
        if (isz == 0 || isz > 1120 || (isz % 16) != 0) continue;
        if (pa < 0x10000 || pa >= g_rdram_size) continue;

        if (g_t3d_slot < 0) {
            g_t3d_slot = ovl;
            g_hle.detect.overlay_id = ovl;
            g_hle.detect.confirmed = true;
            g_hle.detect.confidence = 99;
            T3D_LOG(&g_hle, "T3D_SCAN", "event=overlay_found slot=%d", ovl);
        }
        if (ovl != g_t3d_slot) continue;

        uint32_t w2 = t3d_hle_read32(g_rdram, g_rdram_size, addr + 8);
        if (g_block_count < MAX_BLOCKS) {
            g_blocks[g_block_count].vl_addr = addr;
            g_blocks[g_block_count].vert_count = isz / 16;
            g_blocks[g_block_count].vert_rdram = pa;
            g_blocks[g_block_count].dest_off = (w2 >> 16) & 0xFFFF;
            g_blocks[g_block_count].input_off = w2 & 0xFFFF;
            g_block_count++;
            T3D_LOG(&g_hle, "T3D_SCAN_HIT",
                "idx=%d addr=0x%08X ovl=%d verts=%u vert_rdram=0x%08X",
                g_block_count, addr, ovl, isz / 16, pa);
        }
    }

    T3D_LOG(&g_hle, "T3D_SCAN", "event=anchors blocks=%d ovl=%d", g_block_count, g_t3d_slot);
    if (g_block_count == 0) return;

    for (int bi = 0; bi < g_block_count; bi++) {
        uint32_t vl = g_blocks[bi].vl_addr;
        uint32_t bs = vl;
        for (int b = 1; b <= 64; b++) {
            if (vl < (uint32_t)(b * 4) + 0x10000) break;
            uint32_t pw = t3d_hle_read32(g_rdram, g_rdram_size, vl - b * 4);
            int po = RSPQ_OVL_ID(pw);
            if (po == g_t3d_slot || po == 0) bs = vl - b * 4;
            else break;
        }
        g_blocks[bi].block_start = bs;

        uint32_t pos = bs, end = vl + 2048;
        if (end > g_rdram_size) end = g_rdram_size;
        int cmds = 0;

        T3DHLEFrameStats saved = g_hle.frame;
        memset(&g_hle.frame, 0, sizeof(g_hle.frame));

        while (pos + 3 < end && cmds < 256) {
            uint32_t w = t3d_hle_read32(g_rdram, g_rdram_size, pos);
            if (w == 0) { pos += 4; continue; }
            int o = RSPQ_OVL_ID(w), c = RSPQ_CMD_ID(w);

            if (o == 0 && c == RSPQ_CMD_RET) break;
            if (o == 0) {
                int sz = 4;
                if (c == 0x3) sz = 12;
                else if (c == 0x5) sz = 16;
                else if (c == 0x7 || c == 0x9 || c == 0xA) sz = 12;
                pos += sz; cmds++; continue;
            }
            if (o == g_rdpq_slot && c < 64) {
                pos += rdpq_cmd_sizes[c]; cmds++; continue;
            }
            if (o == g_t3d_slot && c < T3D_CMD_COUNT) {
                int consumed = t3d_hle_process_command(&g_hle, g_rdram, g_rdram_size, pos);
                if (consumed > 0) { pos += consumed * 4; cmds++; continue; }
            }
            pos += 4; cmds++;
        }

        g_blocks[bi].tri_count = g_hle.frame.tris_drawn;
        g_blocks[bi].cmd_count = cmds;

        T3D_LOG(&g_hle, "T3D_BLOCK",
            "idx=%d start=0x%08X cmds=%d verts=%u tris=%d vert_rdram=0x%08X",
            bi, bs, cmds, g_blocks[bi].vert_count, g_blocks[bi].tri_count,
            g_blocks[bi].vert_rdram);

        g_hle.frame = saved;
    }

    int tv = 0, tt = 0;
    for (int i = 0; i < g_block_count; i++) { tv += g_blocks[i].vert_count; tt += g_blocks[i].tri_count; }
    T3D_LOG(&g_hle, "T3D_SCAN", "event=complete blocks=%d verts=%d tris=%d ovl=%d", g_block_count, tv, tt, g_t3d_slot);
}

void t3d_hle_on_vblank(void) {
    if (!g_initialized) return;

    /* Debug: prove new vblank handler is running */
    if (g_hle.total_frames == 79) {
        T3D_LOG(&g_hle, "T3D_VBLANK_NEW", "frame=%llu rdram=%p size=%u",
            (unsigned long long)g_hle.total_frames, (void*)g_rdram, g_rdram_size);
    }

    static bool scanned = false;
    if (!scanned && g_hle.total_frames == 80) {
        scanned = true;
        scan_rdram_for_blocks();
    }
    if (g_block_count == 0 && g_hle.total_frames == 200) {
        scan_rdram_for_blocks();
    }

    if ((g_hle.total_frames % 120) == 0 && g_hle.total_frames > 0) {
        T3D_LOG(&g_hle, "T3D_STATUS", "frame=%llu t3d=%d rdpq=%d blocks=%d",
            (unsigned long long)g_hle.total_frames, g_t3d_slot, g_rdpq_slot, g_block_count);
    }

    t3d_hle_frame_end(&g_hle);
}
