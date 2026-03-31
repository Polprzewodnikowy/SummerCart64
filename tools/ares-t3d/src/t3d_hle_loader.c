/*
 * t3d_hle_loader.c — ares-side plugin loader
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
    if (g_handle) return true;

    const char *paths[8];
    int n = 0;
    if (so_path) paths[n++] = so_path;
    paths[n++] = "./libt3d_hle.so";
    paths[n++] = "libt3d_hle.so";
    paths[n++] = "/home/user/summercart64-experiment/tools/ares-t3d/build/libt3d_hle.so";
    paths[n] = NULL;

    for (int i = 0; paths[i]; i++) {
        if (!paths[i]) continue; /* skip NULL entries */
        g_handle = dlopen(paths[i], RTLD_NOW);
        if (g_handle) {
            fprintf(stderr, "[T3D-HLE] Loaded from: %s\n", paths[i]);
            break;
        }
        const char *err = dlerror();
        fprintf(stderr, "[T3D-HLE] dlopen('%s'): %s\n", paths[i],
                err ? err : "unknown");
    }

    if (!g_handle) {
        fprintf(stderr, "[T3D-HLE] Plugin not found, continuing normally\n");
        return false;
    }

    T3DHLEPluginGetFn get_fn = (T3DHLEPluginGetFn)dlsym(g_handle, T3D_HLE_PLUGIN_ENTRY);
    if (!get_fn) {
        fprintf(stderr, "[T3D-HLE] Missing entry: %s\n", T3D_HLE_PLUGIN_ENTRY);
        dlclose(g_handle); g_handle = NULL;
        return false;
    }

    g_plugin = get_fn();
    if (!g_plugin || g_plugin->version != T3D_HLE_PLUGIN_VERSION) {
        fprintf(stderr, "[T3D-HLE] Version mismatch or NULL\n");
        g_plugin = NULL; dlclose(g_handle); g_handle = NULL;
        return false;
    }

    fprintf(stderr, "[T3D-HLE] Plugin loaded (v%d)\n", g_plugin->version);
    return true;
}

void t3d_plugin_unload(void) {
    if (g_plugin && g_plugin->shutdown) g_plugin->shutdown();
    g_plugin = NULL;
    if (g_handle) { dlclose(g_handle); g_handle = NULL; }
}

bool t3d_plugin_active(void) { return g_plugin != NULL; }

void t3d_plugin_init(uint8_t *d, uint32_t s) { if (g_plugin && g_plugin->init) g_plugin->init(d, s); }
void t3d_plugin_on_dma(uint32_t da, uint32_t pa, uint32_t l, uint32_t r, const uint8_t *rd, uint32_t rs) {
    if (g_plugin && g_plugin->on_dma) g_plugin->on_dma(da, pa, l, r, rd, rs);
}
void t3d_plugin_on_sp_status(uint32_t d) { if (g_plugin && g_plugin->on_sp_status) g_plugin->on_sp_status(d); }
void t3d_plugin_on_vblank(void) { if (g_plugin && g_plugin->on_vblank) g_plugin->on_vblank(); }
void t3d_plugin_on_rdram_write(uint32_t a, uint32_t v) { if (g_plugin && g_plugin->on_rdram_write) g_plugin->on_rdram_write(a, v); }
