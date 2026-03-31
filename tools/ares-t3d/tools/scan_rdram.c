/*
 * scan_rdram.c — Standalone RDRAM scanner for T3D blocks
 *
 * Reads a raw RDRAM dump file and scans for T3D VERT_LOAD patterns.
 * Extracts block geometry and dumps to stdout.
 *
 * Usage: scan_rdram <rdram.bin>
 *
 * To capture RDRAM from ares: pause emulation after level loads,
 * use the debugger to dump 8MB of RDRAM to a file.
 * Or: modify ares to write RDRAM to disk at a known frame.
 */

#include "t3d_hle.h"
#include "overlay_sizes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_BLOCKS 32

typedef struct {
    uint32_t vl_addr;
    uint32_t vert_count;
    uint32_t vert_rdram;
    uint16_t dest_off;
    uint16_t input_off;
    int tri_count;
} BlockInfo;

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <rdram.bin>\n", argv[0]);
        fprintf(stderr, "  Scans raw N64 RDRAM dump for T3D rspq_blocks.\n");
        return 1;
    }

    FILE *fp = fopen(argv[1], "rb");
    if (!fp) { perror("fopen"); return 1; }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (size < 1024 * 1024 || size > 16 * 1024 * 1024) {
        fprintf(stderr, "Invalid RDRAM size: %ld (expected 4-8MB)\n", size);
        fclose(fp);
        return 1;
    }

    uint8_t *rdram = malloc(size);
    if (fread(rdram, 1, size, fp) != (size_t)size) {
        fprintf(stderr, "Read error\n");
        free(rdram); fclose(fp);
        return 1;
    }
    fclose(fp);

    uint32_t rdram_size = (uint32_t)size;
    fprintf(stdout, "RDRAM loaded: %u bytes\n\n", rdram_size);

    /* Initialize parser */
    T3DHLEState hle;
    t3d_hle_init(&hle, 0);
    t3d_hle_set_verbose(&hle, true);
    t3d_hle_set_log(&hle, stdout);

    /* Scan for VERT_LOAD anchors */
    BlockInfo blocks[MAX_BLOCKS];
    int block_count = 0;
    int t3d_ovl = -1;

    fprintf(stdout, "=== Scanning for VERT_LOAD anchors ===\n");

    for (uint32_t addr = 0x10000; addr + 11 < rdram_size; addr += 4) {
        uint32_t w0 = t3d_hle_read32(rdram, rdram_size, addr);
        int ovl = RSPQ_OVL_ID(w0);
        int cmd = RSPQ_CMD_ID(w0);
        if (cmd != T3D_CMD_VERT_LOAD || ovl == 0 || ovl >= 15) continue;

        uint32_t isz = RSPQ_ARGS(w0);
        uint32_t w1 = t3d_hle_read32(rdram, rdram_size, addr + 4);
        uint32_t pa = w1 & 0x1FFFFFFF;
        if (isz == 0 || isz > 1120 || (isz % 16) != 0) continue;
        if (pa < 0x10000 || pa >= rdram_size) continue;

        if (t3d_ovl < 0) {
            t3d_ovl = ovl;
            hle.detect.overlay_id = ovl;
            hle.detect.confirmed = true;
            fprintf(stdout, "T3D overlay detected: slot %d\n", ovl);
        }
        if (ovl != t3d_ovl) continue;

        uint32_t w2 = t3d_hle_read32(rdram, rdram_size, addr + 8);
        if (block_count < MAX_BLOCKS) {
            blocks[block_count].vl_addr = addr;
            blocks[block_count].vert_count = isz / 16;
            blocks[block_count].vert_rdram = pa;
            blocks[block_count].dest_off = (w2 >> 16) & 0xFFFF;
            blocks[block_count].input_off = w2 & 0xFFFF;
            blocks[block_count].tri_count = 0;

            fprintf(stdout, "  Block %d: addr=0x%08X verts=%u vert_rdram=0x%08X\n",
                    block_count, addr, isz / 16, pa);
            block_count++;
        }
    }

    fprintf(stdout, "\nFound %d blocks (overlay %d)\n\n", block_count, t3d_ovl);

    if (block_count == 0) {
        fprintf(stdout, "No T3D blocks found.\n");
        free(rdram);
        return 0;
    }

    /* Parse each block and dump vertex data */
    fprintf(stdout, "=== Parsing blocks ===\n\n");

    for (int bi = 0; bi < block_count; bi++) {
        uint32_t vl = blocks[bi].vl_addr;
        uint32_t pa = blocks[bi].vert_rdram;
        uint32_t nv = blocks[bi].vert_count;

        fprintf(stdout, "--- Block %d: %u vertices at RDRAM 0x%08X ---\n", bi, nv, pa);

        /* Read and dump vertex positions */
        for (uint32_t vi = 0; vi < nv && vi < 70; vi++) {
            /* T3DVertPacked: 16 bytes per vertex
             * Bytes 0-1: x (s16)
             * Bytes 2-3: y (s16)
             * Bytes 4-5: z (s16)
             * Bytes 6-7: normal (packed u16)
             * OR for second vert in pair:
             * Bytes 8-9: x, 10-11: y, 12-13: z, 14-15: normal
             *
             * Actually T3DVertPacked is 32 bytes for 2 verts.
             * Vert A at offset 0, Vert B at offset 8 (positions)
             * UVs at 16-19 (A) and 24-27 (B)
             * RGBA at 20-23 (A) and 28-31 (B)
             */
            uint32_t pair_idx = vi / 2;
            uint32_t pair_addr = pa + pair_idx * 32;
            int is_b = (vi % 2);

            if (pair_addr + 31 >= rdram_size) break;

            int16_t x, y, z;
            uint8_t r, g, b, a;
            if (!is_b) {
                x = (int16_t)t3d_hle_read16(rdram, rdram_size, pair_addr + 0);
                y = (int16_t)t3d_hle_read16(rdram, rdram_size, pair_addr + 2);
                z = (int16_t)t3d_hle_read16(rdram, rdram_size, pair_addr + 4);
                r = rdram[pair_addr + 20];
                g = rdram[pair_addr + 21];
                b = rdram[pair_addr + 22];
                a = rdram[pair_addr + 23];
            } else {
                x = (int16_t)t3d_hle_read16(rdram, rdram_size, pair_addr + 8);
                y = (int16_t)t3d_hle_read16(rdram, rdram_size, pair_addr + 10);
                z = (int16_t)t3d_hle_read16(rdram, rdram_size, pair_addr + 12);
                r = rdram[pair_addr + 28];
                g = rdram[pair_addr + 29];
                b = rdram[pair_addr + 30];
                a = rdram[pair_addr + 31];
            }

            fprintf(stdout, "  v%u: pos=(%d, %d, %d) rgba=(%u, %u, %u, %u)\n",
                    vi, x, y, z, r, g, b, a);
        }
        fprintf(stdout, "\n");
    }

    /* Summary */
    fprintf(stdout, "=== Summary ===\n");
    fprintf(stdout, "T3D overlay: slot %d\n", t3d_ovl);
    fprintf(stdout, "Blocks found: %d\n", block_count);
    int total_verts = 0;
    for (int i = 0; i < block_count; i++) total_verts += blocks[i].vert_count;
    fprintf(stdout, "Total vertices: %d\n", total_verts);

    free(rdram);
    return 0;
}
