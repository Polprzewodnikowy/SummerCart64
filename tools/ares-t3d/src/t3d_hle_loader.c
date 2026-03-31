/*
 * t3d_hle_loader.c — ares-side plugin loader
 *
 * Linked into ares. Uses dlopen to load libt3d_hle.so at runtime.
 * If the .so is missing or incompatible, all hooks become no-ops.
 */

#include "t3d_hle_loader.h"
#include "t3d_hle_plugin.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *g_handle = NULL;
static T3DHLEPlugin *g_plugin = NULL;

bool t3d_plugin_load(const char *so_path) {
    if (g_handle) return true; /* already loaded */

    /* Try the specified path first, then current dir, then LD path */
    const char *paths[] = {
        so_path,
        "./libt3d_hle.so",
        "libt3d_hle.so",
        NULL
    };

    for (int i = 0; paths[i]; i++) {
        g_handle = dlopen(paths[i], RTLD_NOW);
        if (g_handle) break;
    }

    if (!g_handle) {
        fprintf(stderr, "[T3D-HLE] Plugin not found (normal operation continues)\n");
        return false;
    }

    /* Find the entry point */
    T3DHLEPluginGetFn get_fn = (T3DHLEPluginGetFn)dlsym(g_handle, T3D_HLE_PLUGIN_ENTRY);
    if (!get_fn) {
        fprintf(stderr, "[T3D-HLE] Plugin missing entry point: %s\n", T3D_HLE_PLUGIN_ENTRY);
        dlclose(g_handle);
        g_handle = NULL;
        return false;
    }

    g_plugin = get_fn();
    if (!g_plugin) {
        fprintf(stderr, "[T3D-HLE] Plugin returned NULL interface\n");
        dlclose(g_handle);
        g_handle = NULL;
        return false;
    }

    /* Version check */
    if (g_plugin->version != T3D_HLE_PLUGIN_VERSION) {
        fprintf(stderr, "[T3D-HLE] Plugin version mismatch: got %d, need %d\n",
                g_plugin->version, T3D_HLE_PLUGIN_VERSION);
        g_plugin = NULL;
        dlclose(g_handle);
        g_handle = NULL;
        return false;
    }

    fprintf(stderr, "[T3D-HLE] Plugin loaded (version %d)\n", g_plugin->version);
    return true;
}

void t3d_plugin_unload(void) {
    if (g_plugin && g_plugin->shutdown) {
        g_plugin->shutdown();
    }
    g_plugin = NULL;
    if (g_handle) {
        dlclose(g_handle);
        g_handle = NULL;
    }
}

bool t3d_plugin_active(void) {
    return g_plugin != NULL;
}

/* Hook shims — delegate to plugin or no-op */

void t3d_plugin_init(uint8_t *rdram_data, uint32_t rdram_size) {
    if (g_plugin && g_plugin->init)
        g_plugin->init(rdram_data, rdram_size);
}

void t3d_plugin_on_dma(uint32_t dram_addr, uint32_t pbus_addr,
                       uint32_t length, uint32_t region,
                       const uint8_t *rdram, uint32_t rdram_size) {
    if (g_plugin && g_plugin->on_dma)
        g_plugin->on_dma(dram_addr, pbus_addr, length, region, rdram, rdram_size);
}

void t3d_plugin_on_sp_status(uint32_t data) {
    if (g_plugin && g_plugin->on_sp_status)
        g_plugin->on_sp_status(data);
}

void t3d_plugin_on_vblank(void) {
    if (g_plugin && g_plugin->on_vblank)
        g_plugin->on_vblank();
}

void t3d_plugin_on_rdram_write(uint32_t address, uint32_t value) {
    if (g_plugin && g_plugin->on_rdram_write)
        g_plugin->on_rdram_write(address, value);
}
