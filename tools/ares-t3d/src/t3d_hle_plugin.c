/*
 * t3d_hle_plugin.c — Shared library entry point
 *
 * Wraps t3d_hle_hooks.c functions into the versioned plugin interface.
 * Build as: gcc -shared -fPIC -o libt3d_hle.so t3d_hle_plugin.c ...
 */

#include "t3d_hle_plugin.h"
#include "t3d_hle_hooks.h"
#include <stddef.h>

/* Plugin wrapper functions — thin shims around the existing hooks */

static void plugin_init(uint8_t *rdram_data, uint32_t rdram_size) {
    t3d_hle_system_init(rdram_data, rdram_size);
}

static void plugin_shutdown(void) {
    /* No cleanup needed currently */
}

static void plugin_on_dma(uint32_t dram_addr, uint32_t pbus_addr,
                          uint32_t length, uint32_t region,
                          const uint8_t *rdram, uint32_t rdram_size) {
    t3d_hle_on_rsp_dma(dram_addr, pbus_addr, length, region,
                        rdram, rdram_size, NULL);
}

static void plugin_on_sp_status(uint32_t data) {
    t3d_hle_on_sp_status(data);
}

static void plugin_on_vblank(void) {
    t3d_hle_on_vblank();
}

static void plugin_on_rdram_write(uint32_t address, uint32_t value) {
    t3d_hle_on_rdram_write(address, value);
}

/* The interface struct — returned by the entry point */
static T3DHLEPlugin g_plugin = {
    .version       = T3D_HLE_PLUGIN_VERSION,
    .init          = plugin_init,
    .shutdown      = plugin_shutdown,
    .on_dma        = plugin_on_dma,
    .on_sp_status  = plugin_on_sp_status,
    .on_vblank     = plugin_on_vblank,
    .on_rdram_write = plugin_on_rdram_write,
};

/* Exported entry point — ares calls this via dlsym */
__attribute__((visibility("default")))
T3DHLEPlugin* t3d_hle_get_plugin(void) {
    return &g_plugin;
}
