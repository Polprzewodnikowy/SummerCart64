/**
 * LZ4 Block Decompressor for N64 (VR4300)
 *
 * Implements the LZ4 block format decompression algorithm.
 * Reference: https://github.com/lz4/lz4/blob/dev/doc/lz4_Block_format.md
 */

#include "lz4_decode.h"

int lz4_decode_block(const uint8_t *src, uint8_t *dst,
                     int compressed_size, int max_output_size) {
    const uint8_t *ip = src;
    const uint8_t *ip_end = src + compressed_size;
    uint8_t *op = dst;
    uint8_t *op_end = dst + max_output_size;

    while (ip < ip_end) {
        /* Read token */
        unsigned int token = *ip++;
        unsigned int lit_len = token >> 4;
        unsigned int match_len;

        /* Decode literal length */
        if (lit_len == 15) {
            unsigned int s;
            do {
                if (ip >= ip_end) return -1;
                s = *ip++;
                lit_len += s;
            } while (s == 255);
        }

        /* Copy literals */
        if (op + lit_len > op_end) return -1;
        if (ip + lit_len > ip_end) return -1;

        /* Word-aligned copy for better VR4300 performance */
        {
            const uint8_t *lit_src = ip;
            uint8_t *lit_dst = op;
            int remaining = (int)lit_len;

            /* Byte copy for small or unaligned transfers */
            while (remaining > 0) {
                *lit_dst++ = *lit_src++;
                remaining--;
            }
        }
        ip += lit_len;
        op += lit_len;

        /* Check for end of block (last sequence has no match) */
        if (ip >= ip_end) break;

        /* Read match offset (little-endian 16-bit) */
        if (ip + 2 > ip_end) return -1;
        unsigned int offset = ip[0] | ((unsigned int)ip[1] << 8);
        ip += 2;

        if (offset == 0) return -1;
        if (op - dst < (int)offset) return -1;

        /* Decode match length */
        match_len = (token & 0x0F) + 4;  /* minmatch = 4 */
        if ((token & 0x0F) == 15) {
            unsigned int s;
            do {
                if (ip >= ip_end) return -1;
                s = *ip++;
                match_len += s;
            } while (s == 255);
        }

        if (op + match_len > op_end) return -1;

        /* Copy match (may overlap, so must copy byte-by-byte for small offsets) */
        {
            const uint8_t *match_src = op - offset;
            uint8_t *match_dst = op;
            int remaining = (int)match_len;

            if (offset >= 8 && remaining >= 8) {
                /* Non-overlapping: can copy in larger chunks */
                while (remaining >= 8) {
                    match_dst[0] = match_src[0];
                    match_dst[1] = match_src[1];
                    match_dst[2] = match_src[2];
                    match_dst[3] = match_src[3];
                    match_dst[4] = match_src[4];
                    match_dst[5] = match_src[5];
                    match_dst[6] = match_src[6];
                    match_dst[7] = match_src[7];
                    match_src += 8;
                    match_dst += 8;
                    remaining -= 8;
                }
            }
            while (remaining > 0) {
                *match_dst++ = *match_src++;
                remaining--;
            }
        }
        op += match_len;
    }

    return (int)(op - dst);
}
