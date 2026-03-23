/**
 * LZ4 Block Decompressor for N64 (VR4300)
 *
 * Minimal LZ4 block format decoder. Does NOT handle LZ4 frames -
 * expects raw LZ4 block data as produced by lz4_compress_block().
 *
 * Performance target: ~5-10 MB/s output on VR4300 @ 93.75 MHz.
 */

#ifndef LZ4_DECODE_H__
#define LZ4_DECODE_H__

#include <stddef.h>
#include <stdint.h>

/**
 * Decompress a single LZ4 block.
 *
 * @param src           Pointer to compressed data
 * @param dst           Pointer to output buffer (must be large enough)
 * @param compressed_size   Size of compressed input in bytes
 * @param max_output_size   Maximum bytes to write to dst
 * @return              Number of bytes written to dst, or -1 on error
 */
int lz4_decode_block(const uint8_t *src, uint8_t *dst,
                     int compressed_size, int max_output_size);

#endif
