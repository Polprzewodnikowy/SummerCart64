/*
 * t3d_hle_loader.h — ares-side plugin loader
 *
 * Replaces the static #if defined(T3D_HLE) hooks with a runtime
 * dlopen/dlsym approach. If libt3d_hle.so is not found, ares
 * continues normally (all hooks become no-ops).
 *
 * This header is included from ares source files (C++).
 * The implementation is in t3d_hle_loader.c (C, linked into ares).
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* Try to load the plugin. Returns true if loaded successfully. */
bool t3d_plugin_load(const char *so_path);

/* Unload the plugin. */
void t3d_plugin_unload(void);

/* Is the plugin loaded? */
bool t3d_plugin_active(void);

/* Hook functions — call these from ares. They delegate to the .so
 * if loaded, otherwise no-op. */
void t3d_plugin_init(uint8_t *rdram_data, uint32_t rdram_size);
void t3d_plugin_on_dma(uint32_t dram_addr, uint32_t pbus_addr,
                       uint32_t length, uint32_t region,
                       const uint8_t *rdram, uint32_t rdram_size);
void t3d_plugin_on_sp_status(uint32_t data);
void t3d_plugin_on_vblank(void);
void t3d_plugin_on_rdram_write(uint32_t address, uint32_t value);

#ifdef __cplusplus
}
#endif
