/*
 * t3d_hle_plugin.h — Versioned plugin interface for ares-t3d
 *
 * ares loads libt3d_hle.so via dlopen and calls through this
 * interface struct. The .so can be rebuilt independently without
 * touching ares. ares continues normally if the .so is missing.
 *
 * Rules:
 *   - Narrow API: init, shutdown, on_dma, on_sp_status, on_vblank, on_rdram_write
 *   - Versioned: struct has a version field, ares checks compatibility
 *   - Explicit buffers: RDRAM pointer + size passed per call, no caching
 *   - Graceful failure: NULL function pointers = skip that hook
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#define T3D_HLE_PLUGIN_VERSION 1
#define T3D_HLE_PLUGIN_SO "libt3d_hle.so"

/* Plugin interface struct — loaded from .so via dlsym("t3d_hle_interface") */
typedef struct {
    int version;  /* must equal T3D_HLE_PLUGIN_VERSION */

    /* Called once at system load. rdram_data is the emulator's RDRAM buffer. */
    void (*init)(uint8_t *rdram_data, uint32_t rdram_size);

    /* Called at system shutdown. */
    void (*shutdown)(void);

    /* Called after every RSP DMA transfer completes.
     * region: 0=DMEM, 1=IMEM */
    void (*on_dma)(uint32_t dram_addr, uint32_t pbus_addr,
                   uint32_t length, uint32_t region,
                   const uint8_t *rdram, uint32_t rdram_size);

    /* Called when CPU writes to SP_STATUS register. */
    void (*on_sp_status)(uint32_t data);

    /* Called at VI vblank (frame boundary). */
    void (*on_vblank)(void);

    /* Called on every CPU word write to RDRAM.
     * value is in N64 byte order (big-endian). */
    void (*on_rdram_write)(uint32_t address, uint32_t value);

} T3DHLEPlugin;

/* The .so exports this function to return the interface struct.
 * ares calls it once after dlopen. */
typedef T3DHLEPlugin* (*T3DHLEPluginGetFn)(void);

#define T3D_HLE_PLUGIN_ENTRY "t3d_hle_get_plugin"
