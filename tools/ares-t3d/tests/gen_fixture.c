/*
 * gen_fixture.c — Generate synthetic T3D command buffer fixture
 *
 * Writes a binary file containing a realistic sequence of T3D
 * rspq commands (big-endian) for testing the decoder.
 *
 * Usage: gen_fixture <output.bin> [overlay_id]
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void write_be32(FILE *fp, uint32_t val) {
    uint8_t b[4] = { val >> 24, val >> 16, val >> 8, val };
    fwrite(b, 1, 4, fp);
}

static uint32_t rspq_word(int ovl, int cmd, uint32_t args) {
    return ((ovl & 0xF) << 28) | ((cmd & 0xF) << 24) | (args & 0x00FFFFFF);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <output.bin> [overlay_id]\n", argv[0]);
        return 1;
    }

    int ovl = argc >= 3 ? atoi(argv[2]) : 6;
    FILE *fp = fopen(argv[1], "wb");
    if (!fp) { perror("fopen"); return 1; }

    fprintf(stderr, "Generating T3D fixture (overlay=%d)...\n", ovl);

    /* Sequence mimicking TR1 Lara render:
     * 1. SCREEN_SIZE (viewport setup)
     * 2. DRAWFLAGS (depth + shaded)
     * 3. LIGHT_SET (ambient)
     * 4. MATRIX_STACK (push model matrix)
     * 5. VERT_LOAD (load mesh vertices)
     * 6. TRI_DRAW x3 (draw triangles)
     * 7. TRI_SYNC (flush)
     */

    /* SCREEN_SIZE: 5 words */
    write_be32(fp, rspq_word(ovl, 0x1, 0x000400));  /* guardband */
    write_be32(fp, 0x01410F01);  /* offset */
    write_be32(fp, 0x01400F00);  /* scale */
    write_be32(fp, 0x7FFF8000);  /* depth + W scale */
    write_be32(fp, 0x00000000);  /* padding */

    /* DRAWFLAGS: 2 words */
    write_be32(fp, rspq_word(ovl, 0x6, 0x000005));  /* depth | shaded */
    write_be32(fp, 0xC8113000);  /* RDP cmd word */

    /* LIGHT_SET (ambient): 4 words */
    write_be32(fp, rspq_word(ovl, 0x5, 0x000100));  /* DMEM addr for ambient */
    write_be32(fp, 0xB4B4B4FF);  /* RGBA: 180,180,180,255 */
    write_be32(fp, 0x00000000);  /* direction x,y */
    write_be32(fp, 0x00000000);  /* direction z */

    /* MATRIX_STACK (set, multiply): 2 words */
    write_be32(fp, rspq_word(ovl, 0x2, 0x000101));  /* advance=1, multiply=1 */
    write_be32(fp, 0x00200000);  /* matrix addr in RDRAM */

    /* VERT_LOAD: 3 words (load 6 verts = 3 packed pairs = 96 bytes) */
    write_be32(fp, rspq_word(ovl, 0x4, 96));  /* 96 bytes */
    write_be32(fp, 0x00300000);  /* vertex data addr */
    write_be32(fp, 0x00000000);  /* offsets */

    /* TRI_DRAW x3: 2 words each */
    write_be32(fp, rspq_word(ovl, 0x0, 0x000100));  /* v0 DMEM addr */
    write_be32(fp, 0x01240148);  /* v1=0x124, v2=0x148 */

    write_be32(fp, rspq_word(ovl, 0x0, 0x000148));
    write_be32(fp, 0x016C0190);

    write_be32(fp, rspq_word(ovl, 0x0, 0x000100));
    write_be32(fp, 0x01480190);

    /* TRI_SYNC: 1 word */
    write_be32(fp, rspq_word(ovl, 0xA, 0));

    fclose(fp);
    fprintf(stderr, "Written to %s\n", argv[1]);
    return 0;
}
