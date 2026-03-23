/**
 * SC64 Asset Streaming Manager
 *
 * Top-level API for demand-paged asset streaming from SD card.
 * Combines SC64 SD access, LZ4 decompression, and LRU caching.
 *
 * POINTER LIFETIME AND OWNERSHIP
 * ==============================
 *
 * asset_load() returns a raw pointer into SDRAM. This pointer is ONLY
 * guaranteed valid until the next call to asset_load/asset_prefetch/asset_flush.
 * A subsequent load may evict the slot your pointer points to.
 *
 * Two usage patterns are safe:
 *
 * 1. TRANSIENT USE (load, consume, forget):
 *
 *      void *tex = asset_load(&mgr, ASSET_TREE_TEXTURE);
 *      upload_to_tmem(tex, size);   // Use immediately
 *      // tex is now "stale" - don't store it
 *
 * 2. PINNED USE (load, pin, keep pointer, unpin when done):
 *
 *      void *mdl = asset_load(&mgr, ASSET_PLAYER_MODEL);
 *      asset_pin(&mgr, ASSET_PLAYER_MODEL);
 *      // mdl is now safe to store and use across frames.
 *      // It will NEVER be evicted while pinned.
 *      player->model = mdl;
 *      ...
 *      // When truly done (e.g. level unload):
 *      asset_unpin(&mgr, ASSET_PLAYER_MODEL);
 *      player->model = NULL;
 *
 * Pin counts are reference-counted. Two pins require two unpins.
 * Pinned assets survive cache_flush() but not cache_flush_all().
 *
 * RULES:
 * - Never store a pointer from asset_load() without pinning it first.
 * - Always unpin when done. Leaked pins eat cache slots forever.
 * - asset_is_cached() does NOT prevent eviction. Only pin does.
 * - Prefetch + pin is the ideal pattern for "I'll need this soon."
 *
 * Usage:
 *
 *   // At boot:
 *   asset_manager_t mgr;
 *   asset_init(&mgr, &(asset_init_config_t){
 *       .archive_base_sector = 0x10000,
 *       .cache_base = (uint8_t *)0xB1000000,
 *       .cache_size = 48 * 1024 * 1024,
 *       .slot_size  = 2 * 1024 * 1024,
 *       .staging_buffer = staging_buf,
 *       .staging_size = 2 * 1024 * 1024,
 *   });
 *
 *   // Pin long-lived assets:
 *   void *player = asset_load(&mgr, ASSET_PLAYER_MODEL);
 *   asset_pin(&mgr, ASSET_PLAYER_MODEL);  // Safe to keep pointer
 *
 *   // Transient use (load, consume, discard):
 *   void *tex = asset_load(&mgr, ASSET_LEVEL1_TEXTURES);
 *   upload_to_tmem(tex, size);  // Don't store tex
 *
 *   // Cleanup:
 *   asset_unpin(&mgr, ASSET_PLAYER_MODEL);
 *   asset_shutdown(&mgr);
 */

#ifndef ASSET_MANAGER_H__
#define ASSET_MANAGER_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "asset_archive.h"
#include "asset_cache.h"

/* Maximum slots supported */
#define ASSET_MAX_SLOTS     (64)

/* Error codes */
typedef enum {
    ASSET_OK = 0,
    ASSET_ERR_SD_INIT,          /* Failed to initialize SD card */
    ASSET_ERR_SD_READ,          /* SD card read error */
    ASSET_ERR_BAD_ARCHIVE,      /* Invalid archive header */
    ASSET_ERR_VERSION,          /* Unsupported archive version */
    ASSET_ERR_TOO_MANY_ASSETS,  /* Asset count exceeds ASSET_MAX_ENTRIES */
    ASSET_ERR_NOT_FOUND,        /* Asset ID not in index */
    ASSET_ERR_DECOMPRESS,       /* LZ4 decompression failed */
    ASSET_ERR_TOO_LARGE,        /* Asset larger than slot size */
    ASSET_ERR_CACHE_FULL,       /* All cache slots busy (loading) */
    ASSET_ERR_NOT_INITIALIZED,  /* Manager not initialized */
} asset_error_t;

/**
 * SD backend interface. Allows swapping the real SC64 hardware
 * for a host-side file backend during testing.
 *
 * Set to NULL in asset_init_config_t to use real SC64 hardware.
 */
typedef struct {
    uint32_t (*init)(void *ctx);
    uint32_t (*deinit)(void *ctx);
    uint32_t (*read_sectors)(void *ctx, void *dest, uint32_t sector, uint32_t count);
    void *ctx;  /* Opaque context (e.g. FILE*, fd, etc.) */
} asset_sd_backend_t;

/* Initialization config */
typedef struct {
    uint32_t    archive_base_sector;    /* SD sector where archive starts */
    uint8_t    *cache_base;             /* SDRAM address for cache region */
    uint32_t    cache_size;             /* Total cache region size */
    uint32_t    slot_size;              /* Size per cache slot */
    uint8_t    *staging_buffer;         /* Temp buffer for compressed reads */
    uint32_t    staging_size;           /* Size of staging buffer */
    const asset_sd_backend_t *backend;  /* NULL = real SC64 hardware */
} asset_init_config_t;

/* Manager instance */
typedef struct asset_manager_t {
    bool                initialized;
    uint32_t            archive_base_sector;    /* SD card base sector */
    asset_archive_header_t  header;             /* Cached archive header */
    asset_entry_t      *index;                  /* Asset index table (in RDRAM) */
    uint32_t            index_count;            /* Number of index entries */
    asset_cache_t       cache;                  /* LRU cache */
    cache_slot_t        slot_storage[ASSET_MAX_SLOTS];  /* Cache slot array */
    uint8_t            *staging_buffer;         /* Compressed data staging area */
    uint32_t            staging_size;
    asset_error_t       last_error;             /* Last error for debugging */
    const asset_sd_backend_t *backend;          /* SD backend (NULL = SC64 hw) */
} asset_manager_t;

/**
 * Initialize the asset manager. Reads archive header and index from SD.
 * SC64 must be unlocked before calling this.
 *
 * @param mgr       Manager instance
 * @param config    Initialization parameters
 * @return          ASSET_OK on success
 */
asset_error_t asset_init(asset_manager_t *mgr, const asset_init_config_t *config);

/**
 * Load an asset by ID. Returns pointer to decompressed data in SDRAM.
 * On cache hit, returns immediately. On miss, loads from SD and decompresses.
 * This is a BLOCKING call.
 *
 * @param mgr       Manager instance
 * @param asset_id  Asset to load
 * @return          Pointer to asset data, or NULL on error (check mgr->last_error)
 */
void *asset_load(asset_manager_t *mgr, uint32_t asset_id);

/**
 * Load an asset and also return its decompressed size.
 *
 * @param mgr       Manager instance
 * @param asset_id  Asset to load
 * @param out_size  Receives the decompressed size in bytes
 * @return          Pointer to asset data, or NULL on error
 */
void *asset_load_with_size(asset_manager_t *mgr, uint32_t asset_id, uint32_t *out_size);

/**
 * Prefetch an asset into cache. Currently blocking (loads synchronously).
 * Future versions may support async DMA loading.
 *
 * @param mgr       Manager instance
 * @param asset_id  Asset to prefetch
 * @return          ASSET_OK on success
 */
asset_error_t asset_prefetch(asset_manager_t *mgr, uint32_t asset_id);

/**
 * Pin an asset in cache. The asset must already be loaded (call asset_load first).
 * Pinned assets cannot be evicted by subsequent loads. Pin calls are
 * reference-counted: each pin must be balanced by an unpin.
 *
 * @param mgr       Manager instance
 * @param asset_id  Asset to pin (must be loaded)
 * @return          true if pinned successfully, false if not in cache
 */
bool asset_pin(asset_manager_t *mgr, uint32_t asset_id);

/**
 * Unpin an asset. Once pin_count reaches 0, the asset becomes evictable again.
 *
 * @param mgr       Manager instance
 * @param asset_id  Asset to unpin
 * @return          true if unpinned successfully
 */
bool asset_unpin(asset_manager_t *mgr, uint32_t asset_id);

/**
 * Load and pin in one call. Convenience for the common pattern:
 *   void *p = asset_load(mgr, id);
 *   asset_pin(mgr, id);
 *
 * @param mgr       Manager instance
 * @param asset_id  Asset to load and pin
 * @return          Pointer to pinned asset data, or NULL on error
 */
void *asset_load_pinned(asset_manager_t *mgr, uint32_t asset_id);

/**
 * Check if an asset is currently pinned.
 */
bool asset_is_pinned(asset_manager_t *mgr, uint32_t asset_id);

/**
 * Check if an asset is currently in cache (without loading it).
 * WARNING: This does NOT prevent eviction. Use asset_pin for that.
 *
 * @param mgr       Manager instance
 * @param asset_id  Asset to check
 * @return          true if asset is cached and ready
 */
bool asset_is_cached(asset_manager_t *mgr, uint32_t asset_id);

/**
 * Evict all non-pinned cached assets. Pinned assets are preserved.
 */
void asset_flush(asset_manager_t *mgr);

/**
 * Shut down the asset manager and release the SD card.
 */
void asset_shutdown(asset_manager_t *mgr);

/**
 * Get a human-readable error string.
 */
const char *asset_error_string(asset_error_t error);

#endif
