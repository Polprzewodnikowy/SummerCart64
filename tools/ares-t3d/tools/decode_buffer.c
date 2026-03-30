/*
 * decode_buffer.c — Standalone rspq/T3D command buffer decoder
 *
 * Reads a binary file containing raw rspq command words (big-endian)
 * and decodes them using the T3D HLE parser.
 *
 * Usage: decode_buffer <buffer.bin> [overlay_id]
 *   buffer.bin:  raw big-endian 32-bit words (e.g. captured from RDRAM)
 *   overlay_id:  force T3D overlay ID (default: auto-detect)
 *
 * Output: machine-readable T3D_CMD / T3D_FRAME lines to stdout.
 */

#include "t3d_hle.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <buffer.bin> [overlay_id]\n", argv[0]);
        return 1;
    }

    FILE *fp = fopen(argv[1], "rb");
    if (!fp) {
        fprintf(stderr, "Cannot open %s\n", argv[1]);
        return 1;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (size < 4 || size > 1024 * 1024) {
        fprintf(stderr, "Invalid size: %ld bytes\n", size);
        fclose(fp);
        return 1;
    }

    /* Allocate fake RDRAM — place buffer at offset 0x1000 */
    uint32_t rdram_size = 8 * 1024 * 1024;
    uint8_t *rdram = calloc(1, rdram_size);
    uint32_t buf_offset = 0x1000;

    if (fread(rdram + buf_offset, 1, size, fp) != (size_t)size) {
        fprintf(stderr, "Read error\n");
        free(rdram);
        fclose(fp);
        return 1;
    }
    fclose(fp);

    T3DHLEState state;
    t3d_hle_init(&state, 0);
    t3d_hle_set_verbose(&state, true);
    t3d_hle_set_log(&state, stdout);

    /* Force overlay if specified */
    if (argc >= 3) {
        int ovl = atoi(argv[2]);
        state.detect.overlay_id = ovl;
        state.detect.confirmed = true;
        state.detect.confidence = 99;
        fprintf(stdout, "T3D_DECODE file=%s size=%ld forced_overlay=%d\n",
                argv[1], size, ovl);
    } else {
        fprintf(stdout, "T3D_DECODE file=%s size=%ld overlay=auto\n",
                argv[1], size);
    }

    /* Scan the buffer */
    uint32_t pos = buf_offset;
    uint32_t end = buf_offset + (uint32_t)size;
    int cmd_count = 0;

    while (pos + 3 < end) {
        uint32_t w0 = t3d_hle_read32(rdram, rdram_size, pos);
        if (w0 == 0) { pos += 4; continue; }

        int ovl_id = RSPQ_OVL_ID(w0);
        int cmd_id = RSPQ_CMD_ID(w0);

        if (ovl_id == 0) {
            fprintf(stdout, "T3D_CMD RSPQ_%d addr=0x%08X w0=0x%08X\n",
                    cmd_id, pos, w0);
            pos += 4; /* conservative: 1 word */
            if (cmd_id == RSPQ_CMD_JUMP || cmd_id == RSPQ_CMD_CALL)
                pos += 4; /* 2 words */
            cmd_count++;
            continue;
        }

        int consumed = t3d_hle_process_command(&state, rdram, rdram_size, pos);
        if (consumed > 0) {
            pos += consumed * 4;
        } else {
            fprintf(stdout, "T3D_CMD UNKNOWN addr=0x%08X ovl=%d cmd=%d w0=0x%08X\n",
                    pos, ovl_id, cmd_id, w0);
            pos += 4;
        }
        cmd_count++;

        if (cmd_count > 1000) {
            fprintf(stdout, "T3D_WARN msg=\"safety limit: 1000 commands\"\n");
            break;
        }
    }

    t3d_hle_frame_end(&state);
    fprintf(stdout, "T3D_DECODE_DONE cmds=%d\n", cmd_count);

    free(rdram);
    return 0;
}
