/*
 * cpu_intercept.h — Path B: CPU-side rspq command interception
 *
 * Instead of decoding the RSP's DMA stream after the fact, this
 * intercepts the N64 CPU's writes to the rspq command buffer in RDRAM.
 *
 * Hook point: SP_STATUS register write with SIG_MORE signal.
 * When the CPU sets SIG_MORE, it has just finished writing one or
 * more rspq commands to the RDRAM buffer. We can then read the
 * buffer region between the last-known position and the new position
 * to capture exactly what was written.
 *
 * This gives us:
 *   - Correct command boundaries (CPU writes whole commands atomically)
 *   - Live vs block context (block recording uses a separate buffer)
 *   - Perfect alignment (no mixed-chunk ambiguity)
 */

#pragma once

#include "t3d_hle.h"
#include "overlay_sizes.h"
#include <stdint.h>
#include <stdbool.h>

/* State for CPU-side command tracking */
typedef struct {
    /* Known rspq buffer regions in RDRAM (detected from first DMA) */
    uint32_t buf_lo_addr[2];    /* low-priority double buffers */
    uint32_t buf_hi_addr[2];    /* high-priority double buffers */
    uint32_t buf_lo_size;       /* size of each lowpri buffer (words) */
    uint32_t buf_hi_size;       /* size of each highpri buffer (words) */
    bool bufs_detected;

    /* Write pointer tracking */
    uint32_t last_scan_addr;    /* last RDRAM address we scanned up to */
    uint32_t scan_fence;        /* don't scan past this */

    /* Block recording state */
    bool in_block;              /* CPU is currently recording a block */
    uint32_t block_start_addr;  /* RDRAM start of current block */

    /* Overlay knowledge */
    int rdpq_slot;
    int t3d_slot;

    /* Stats */
    uint32_t commands_captured;
    uint32_t blocks_captured;
    uint32_t calls_seen;
} T3DCpuIntercept;

/* Initialize CPU intercept state */
void t3d_cpu_init(T3DCpuIntercept *ci);

/* Called when CPU writes to SP_STATUS register.
 * data: the value written to SP_STATUS.
 * This is the primary trigger for scanning new commands. */
void t3d_cpu_on_sp_status_write(T3DCpuIntercept *ci,
                                T3DHLEState *hle,
                                uint32_t data,
                                const uint8_t *rdram,
                                uint32_t rdram_size);

/* Called when RSP DMA loads a command buffer into DMEM.
 * Used to detect the rspq buffer RDRAM addresses. */
void t3d_cpu_on_dma_to_dmem(T3DCpuIntercept *ci,
                            uint32_t dram_addr,
                            uint32_t pbus_addr,
                            uint32_t length);

/* Scan a known rspq buffer region for new commands.
 * start/end are RDRAM physical addresses.
 * Returns number of T3D commands found. */
int t3d_cpu_scan_buffer(T3DCpuIntercept *ci,
                        T3DHLEState *hle,
                        const uint8_t *rdram, uint32_t rdram_size,
                        uint32_t start, uint32_t end);
