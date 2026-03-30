/*
 * t3d_hle_hooks.h — Entry points called from ares hook patches
 *
 * These are the C functions that ares calls via #if defined(T3D_HLE).
 * They delegate to the T3DHLEState singleton.
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Called from system.cpp after rdram is initialized.
 * Sets up the HLE singleton. */
void t3d_hle_system_init(uint8_t *rdram_data, uint32_t rdram_size);

/* Called from rsp/dma.cpp after each DMA transfer completes.
 * region: 0=DMEM, 1=IMEM
 * dram_addr: source address in RDRAM
 * pbus_addr: destination address in RSP memory
 * length: transfer length in bytes
 * rdram/rdram_size: full RDRAM pointer and size
 * region_data: pointer to DMEM or IMEM buffer (post-transfer) */
void t3d_hle_on_rsp_dma(uint32_t dram_addr, uint32_t pbus_addr,
                         uint32_t length, uint32_t region,
                         const uint8_t *rdram, uint32_t rdram_size,
                         const uint8_t *region_data);

/* Called from vi.cpp at vblank (frame boundary). */
void t3d_hle_on_vblank(void);

#ifdef __cplusplus
}
#endif
