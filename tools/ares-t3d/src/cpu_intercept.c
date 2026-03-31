/*
 * cpu_intercept.c — Path B: CPU-side rspq command interception
 *
 * Captures rspq commands by monitoring the CPU's interaction with
 * the RSP, not by decoding the RSP's DMA stream.
 *
 * Primary hook: SP_STATUS write with SIG_MORE (signal 7).
 * Secondary: DMA-to-DMEM for buffer address detection.
 *
 * Uses the rdpq command size table for correct stepping, same as
 * the stream_walker, but with much cleaner input data.
 */

#include "cpu_intercept.h"
#include "overlay_sizes.h"
#include <string.h>
#include <stdio.h>

/* SP_STATUS signal bits (from n64sys.h) */
#define SP_WSTATUS_SET_SIG_MORE    (1 << 24)  /* bit 24: set signal 7 */
#define SP_WSTATUS_CLEAR_SIG_MORE  (1 << 23)  /* bit 23: clear signal 7 */

/* rspq RDRAM buffer size in bytes (from rspq_constants.h) */
#define RSPQ_BUF_SIZE_BYTES  (0x200 * 4)  /* 0x200 words = 2KB */

void t3d_cpu_init(T3DCpuIntercept *ci) {
    memset(ci, 0, sizeof(*ci));
    ci->rdpq_slot = -1;
    ci->t3d_slot = -1;
    ci->buf_lo_size = 0x200; /* words */
    ci->buf_hi_size = 0x80;  /* words */
}

/* ----------------------------------------------------------------
 * Buffer address detection from DMA patterns
 *
 * When the RSP DMAs rspq buffer data from RDRAM to DMEM at low
 * addresses (pbus < 0x100), the source RDRAM address IS the rspq
 * buffer. Track these addresses to know where to scan.
 * ---------------------------------------------------------------- */

void t3d_cpu_on_dma_to_dmem(T3DCpuIntercept *ci,
                            uint32_t dram_addr,
                            uint32_t pbus_addr,
                            uint32_t length) {
    if (ci->bufs_detected) return;
    if (pbus_addr >= 0x100 || length < 32) return;

    uint32_t phys = dram_addr & 0x1FFFFFFF;

    /* Track unique buffer base addresses (aligned to buffer size) */
    uint32_t base = phys & ~(RSPQ_BUF_SIZE_BYTES - 1);

    for (int i = 0; i < 2; i++) {
        if (ci->buf_lo_addr[i] == base) return; /* already known */
        if (ci->buf_lo_addr[i] == 0) {
            ci->buf_lo_addr[i] = base;
            return;
        }
    }
}

/* ----------------------------------------------------------------
 * SP_STATUS write handler
 *
 * When the CPU sets SIG_MORE, it has just written commands to the
 * rspq RDRAM buffer. We scan the buffer for new commands.
 *
 * The tricky part: we need to know WHERE in the buffer the new
 * commands are. The rspq_cur_pointer value is in the N64's address
 * space (a global variable). We can't easily read it from ares.
 *
 * Workaround: on each SIG_MORE, scan the entire active buffer for
 * non-zero words we haven't seen before. This is O(buffer_size)
 * per signal, but the buffers are only 2KB so it's cheap.
 *
 * Better approach: scan from last_scan_addr to the end of the buffer,
 * wrapping around if needed. The rspq double-buffers, so when the
 * RSP finishes one buffer, it signals BUFDONE and the CPU starts
 * writing to the other.
 * ---------------------------------------------------------------- */

void t3d_cpu_on_sp_status_write(T3DCpuIntercept *ci,
                                T3DHLEState *hle,
                                uint32_t data,
                                const uint8_t *rdram,
                                uint32_t rdram_size) {
    /* Only trigger on SIG_MORE being set */
    if (!(data & SP_WSTATUS_SET_SIG_MORE)) return;

    /* Scan all known buffer regions for new commands */
    for (int i = 0; i < 2; i++) {
        uint32_t base = ci->buf_lo_addr[i];
        if (base == 0) continue;
        if (base + RSPQ_BUF_SIZE_BYTES > rdram_size) continue;

        int found = t3d_cpu_scan_buffer(ci, hle, rdram, rdram_size,
                                         base, base + RSPQ_BUF_SIZE_BYTES);
        if (found > 0) {
            ci->commands_captured += found;
        }
    }
}

/* ----------------------------------------------------------------
 * Buffer scanner — walks commands with correct sizes
 * ---------------------------------------------------------------- */

int t3d_cpu_scan_buffer(T3DCpuIntercept *ci,
                        T3DHLEState *hle,
                        const uint8_t *rdram, uint32_t rdram_size,
                        uint32_t start, uint32_t end) {
    int t3d_found = 0;
    uint32_t pos = start;

    while (pos + 3 < end) {
        uint32_t w0 = t3d_hle_read32(rdram, rdram_size, pos);
        if (w0 == 0) { pos += 4; continue; }

        int ovl = RSPQ_OVL_ID(w0);
        int cmd = RSPQ_CMD_ID(w0);

        /* rspq internal (overlay 0) — known sizes */
        if (ovl == 0) {
            int sz;
            switch (cmd) {
            case 0x0: sz = 4; break;   /* INVALID */
            case 0x1: sz = 4; break;   /* NOOP */
            case 0x2: sz = 4; break;   /* JUMP — also changes scan target */
            case 0x3: sz = 12; break;  /* CALL */
            case 0x4: sz = 4; break;   /* RET */
            case 0x5: sz = 16; break;  /* DMA */
            case 0x6: sz = 4; break;   /* WRITE_STATUS */
            case 0x7: sz = 12; break;  /* SWAP_BUFFERS */
            case 0x8: sz = 4; break;   /* TEST_WRITE_STATUS */
            case 0x9: sz = 12; break;  /* RDP_SET_BUFFER */
            case 0xA: sz = 12; break;  /* RDP_APPEND_BUFFER */
            default:  sz = 4; break;
            }

            /* JUMP: changes the buffer read position */
            if (cmd == 0x2) {
                /* Jump target is in the args field (24-bit RDRAM addr) */
                /* For now, just step past. Following jumps would
                 * require tracking the active buffer. */
            }

            /* CALL: this is rspq_block_run! */
            if (cmd == 0x3 && pos + 7 < end) {
                uint32_t block_addr = t3d_hle_read32(rdram, rdram_size, pos + 4) & 0x1FFFFFFF;
                ci->calls_seen++;

                /* Log CALL targets */
                static int call_log = 0;
                if (call_log < 20) {
                    T3D_LOG(hle, "T3D_CPU_CALL",
                        "block_addr=0x%08X w0=0x%08X",
                        block_addr, w0);
                    call_log++;
                }

                /* Scan the block content! This is where VERT_LOAD lives. */
                if (block_addr > 0x1000 && block_addr + 64 < rdram_size) {
                    int block_found = t3d_cpu_scan_buffer(
                        ci, hle, rdram, rdram_size,
                        block_addr, block_addr + 4096); /* max 4KB per block */
                    t3d_found += block_found;
                }
            }

            pos += sz;
            continue;
        }

        /* rdpq overlay — step correctly */
        if (ci->rdpq_slot >= 0 && ovl == ci->rdpq_slot) {
            int sz = (cmd < 64) ? rdpq_cmd_sizes[cmd] : 8;
            pos += sz;
            continue;
        }

        /* T3D overlay — parse */
        if (ci->t3d_slot >= 0 && ovl == ci->t3d_slot) {
            int consumed = t3d_hle_process_command(hle, rdram, rdram_size, pos);
            if (consumed > 0) {
                t3d_found++;
                pos += consumed * 4;
                continue;
            }
        }

        /* Unknown overlay — try T3D detection */
        if (ci->t3d_slot < 0 && cmd < 13) {
            int consumed = t3d_hle_process_command(hle, rdram, rdram_size, pos);
            if (consumed > 0) {
                if (hle->detect.confirmed) {
                    ci->t3d_slot = hle->detect.overlay_id;
                    T3D_LOG(hle, "T3D_CPU", "event=t3d_slot_found slot=%d", ci->t3d_slot);
                }
                t3d_found++;
                pos += consumed * 4;
                continue;
            }
        }

        /* Unknown — skip 4 bytes */
        pos += 4;
    }

    return t3d_found;
}
