/**
 * SC64 Asset Streaming - Single Include
 *
 * Include this one file to get the entire asset streaming API.
 *
 * Quick start:
 *
 *   #include "asset.h"
 *
 *   // Declare storage (one-time, at file scope):
 *   ASSET_STORAGE(mgr, staging, 2 * 1024 * 1024);
 *
 *   // Init:
 *   ASSET_INIT(mgr, staging, 0x10000, 48*MB, 2*MB);
 *
 *   // Load (transient - use immediately, don't store):
 *   void *tex = ASSET(ASSET_LEVEL1_TEX);
 *
 *   // Load (pinned - safe to store across frames):
 *   asset_handle_t h = ASSET_ACQUIRE(ASSET_PLAYER_MODEL);
 *   void *mdl = ASSET_RESOLVE(h);
 *   ASSET_RELEASE(h);
 *
 *   // Shutdown:
 *   asset_shutdown(&mgr);
 */

#ifndef ASSET_H__
#define ASSET_H__

#include "asset_manager.h"
#include "asset_debug.h"

/* Size constants */
#define KB (1024)
#define MB (1024 * 1024)

/**
 * Declare asset manager and staging buffer storage.
 * Place at file scope.
 *
 * @param mgr_name      Name for the asset_manager_t variable
 * @param staging_name   Name for the staging buffer
 * @param staging_bytes  Size of staging buffer
 */
#define ASSET_STORAGE(mgr_name, staging_name, staging_bytes)    \
    static asset_manager_t mgr_name;                            \
    static uint8_t staging_name[(staging_bytes)]                 \
        __attribute__((aligned(16)))

/**
 * Initialize with defaults suitable for SC64 hardware.
 * Assumes SDRAM cache starts at 0x11000000 (after 16 MB resident).
 *
 * @param mgr_name        Manager variable name
 * @param staging_name     Staging buffer variable name
 * @param base_sector      SD sector where archive starts
 * @param cache_bytes      Total cache region size
 * @param slot_bytes       Per-slot size
 */
#define ASSET_INIT(mgr_name, staging_name, base_sector, cache_bytes, slot_bytes) \
    asset_init(&(mgr_name), &(asset_init_config_t){                              \
        .archive_base_sector = (base_sector),                                    \
        .cache_base = (uint8_t *)0x11000000,                                     \
        .cache_size = (cache_bytes),                                             \
        .slot_size  = (slot_bytes),                                              \
        .staging_buffer = (staging_name),                                        \
        .staging_size = sizeof(staging_name),                                    \
        .backend = NULL,                                                         \
    })

/**
 * Shorthand load (transient). Uses the default manager name "mgr".
 * WARNING: Returned pointer may go stale on next load. Use immediately.
 */
#define ASSET(id)               asset_load(&mgr, (id))
#define ASSET_SIZE(id, szp)     asset_load_with_size(&mgr, (id), (szp))

/**
 * Handle-based API shorthands. Uses default manager "mgr".
 */
#define ASSET_ACQUIRE(id)       asset_acquire(&mgr, (id))
#define ASSET_RESOLVE(h)        asset_resolve(&mgr, (h))
#define ASSET_RESOLVE_SIZE(h,s) asset_resolve_with_size(&mgr, (h), (s))
#define ASSET_RELEASE(h)        asset_release(&mgr, (h))

/**
 * Prefetch shorthand.
 */
#define ASSET_PREFETCH(id)      asset_prefetch(&mgr, (id))

#endif
