/**
 * SC64 Asset Streaming Manager
 *
 * Top-level API for demand-paged asset streaming from SD card.
 * Combines SC64 SD access, LZ4 decompression, and LRU caching.
 *
 * Usage:
 *
 *   // At boot:
 *   asset_manager_t mgr;
 *   asset_init(&mgr, &(asset_init_config_t){
 *       .archive_base_sector = 0x10000,    // Where your archive starts on SD
 *       .cache_base = (uint8_t *)0xB1000000,  // SDRAM after 16 MB resident
 *       .cache_size = 48 * 1024 * 1024,       // 48 MB cache
 *       .slot_size  = 2 * 1024 * 1024,        // 2 MB per slot
 *       .staging_buffer = staging_buf,         // Temp buffer for compressed data
 *       .staging_size = 2 * 1024 * 1024,       // Must be >= slot_size
 *   });
 *
 *   // During gameplay:
 *   void *tex = asset_load(&mgr, ASSET_LEVEL1_TEXTURES);  // Returns pointer or NULL
 *   uint32_t size;
 *   void *mdl = asset_load_with_size(&mgr, ASSET_PLAYER_MODEL, &size);
 *
 *   // Prefetch hint (non-blocking if possible, blocking fallback):
 *   asset_prefetch(&mgr, ASSET_LEVEL2_TEXTURES);
 *
 *   // Cleanup:
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

/* Initialization config */
typedef struct {
    uint32_t    archive_base_sector;    /* SD sector where archive starts */
    uint8_t    *cache_base;             /* SDRAM address for cache region */
    uint32_t    cache_size;             /* Total cache region size */
    uint32_t    slot_size;              /* Size per cache slot */
    uint8_t    *staging_buffer;         /* Temp buffer for compressed reads */
    uint32_t    staging_size;           /* Size of staging buffer */
} asset_init_config_t;

/* Manager instance */
typedef struct {
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
 * Check if an asset is currently in cache (without loading it).
 *
 * @param mgr       Manager instance
 * @param asset_id  Asset to check
 * @return          true if asset is cached and ready
 */
bool asset_is_cached(asset_manager_t *mgr, uint32_t asset_id);

/**
 * Evict all cached assets.
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
