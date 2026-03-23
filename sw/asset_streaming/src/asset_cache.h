/**
 * LRU Asset Cache Manager for SC64
 *
 * Manages a region of SDRAM as an LRU cache of fixed-size slots.
 * Each slot can hold one decompressed asset. Eviction is LRU-based.
 *
 * Memory layout (N64 PI address space):
 *
 *   0x1000_0000 + resident_size ... 0x13FF_FFFF
 *   |  Slot 0  |  Slot 1  |  Slot 2  |  ...  |  Slot N  |
 *
 * The cache does NOT manage the resident region - that's the game's
 * responsibility. The cache only manages the streaming window above it.
 */

#ifndef ASSET_CACHE_H__
#define ASSET_CACHE_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Cache slot states */
typedef enum {
    SLOT_EMPTY = 0,     /* No asset loaded */
    SLOT_LOADING,       /* DMA in progress */
    SLOT_READY,         /* Asset decompressed and ready */
} slot_state_t;

/* Single cache slot */
typedef struct {
    uint32_t    asset_id;       /* Which asset is in this slot (0 = empty) */
    slot_state_t state;         /* Current state */
    uint32_t    lru_counter;    /* For LRU eviction (higher = more recently used) */
    uint8_t    *data;           /* Pointer to slot memory (PI-mapped SDRAM) */
    uint32_t    data_size;      /* Decompressed size of current asset */
    uint32_t    pin_count;      /* >0 means slot is pinned and cannot be evicted */
    uint32_t    generation;     /* Bumped on eviction, used for stale handle detection */
} cache_slot_t;

/* Cache configuration */
typedef struct {
    uint8_t    *base_address;   /* Start of cache region in SDRAM (PI address) */
    uint32_t    total_size;     /* Total cache region size in bytes */
    uint32_t    slot_size;      /* Size of each slot in bytes */
    uint32_t    slot_count;     /* Number of slots (total_size / slot_size) */
} cache_config_t;

/* Cache instance */
typedef struct {
    cache_config_t  config;
    cache_slot_t   *slots;          /* Array of slot_count slots (in N64 RDRAM) */
    uint32_t        lru_clock;      /* Monotonic counter for LRU tracking */
    uint32_t        hit_count;      /* Stats: cache hits */
    uint32_t        miss_count;     /* Stats: cache misses */
    uint32_t        pin_count;      /* Stats: total pinned slots */
} asset_cache_t;

/**
 * Initialize the asset cache.
 *
 * @param cache         Cache instance to initialize
 * @param slots         Pre-allocated array of cache_slot_t (in RDRAM)
 * @param base_address  Start of SDRAM cache region (PI address, e.g. 0x11000000)
 * @param total_size    Size of cache region in bytes
 * @param slot_size     Size of each slot in bytes (should match max asset size)
 */
void cache_init(asset_cache_t *cache, cache_slot_t *slots,
                uint8_t *base_address, uint32_t total_size, uint32_t slot_size);

/**
 * Look up an asset in the cache.
 *
 * @param cache     Cache instance
 * @param asset_id  Asset to look up
 * @return          Pointer to cached data, or NULL if not cached
 */
void *cache_lookup(asset_cache_t *cache, uint32_t asset_id);

/**
 * Find or allocate a slot for an asset. If the asset is already cached,
 * returns that slot. Otherwise evicts the LRU slot and returns it.
 *
 * @param cache     Cache instance
 * @param asset_id  Asset that needs a slot
 * @param evicted   Set to true if a slot was evicted (caller may need to load data)
 * @return          Slot index, or -1 on error
 */
int cache_allocate(asset_cache_t *cache, uint32_t asset_id, bool *evicted);

/**
 * Mark a slot as ready (loading complete).
 *
 * @param cache     Cache instance
 * @param slot_idx  Slot index
 * @param data_size Actual decompressed size of the asset
 */
void cache_slot_ready(asset_cache_t *cache, int slot_idx, uint32_t data_size);

/**
 * Get pointer to a slot's data buffer.
 *
 * @param cache     Cache instance
 * @param slot_idx  Slot index
 * @return          Pointer to slot's SDRAM buffer
 */
void *cache_slot_data(asset_cache_t *cache, int slot_idx);

/**
 * Pin an asset in cache, preventing eviction. Pin calls are reference-counted:
 * each pin must be balanced by an unpin. Returns true if asset was found and pinned.
 *
 * @param cache     Cache instance
 * @param asset_id  Asset to pin
 * @return          true if asset was found and pinned, false if not in cache
 */
bool cache_pin(asset_cache_t *cache, uint32_t asset_id);

/**
 * Unpin an asset, allowing it to be evicted again once pin_count reaches 0.
 * Returns true if the asset was found and unpinned.
 *
 * @param cache     Cache instance
 * @param asset_id  Asset to unpin
 * @return          true if asset was found and unpinned, false if not in cache
 */
bool cache_unpin(asset_cache_t *cache, uint32_t asset_id);

/**
 * Check if an asset is currently pinned.
 */
bool cache_is_pinned(asset_cache_t *cache, uint32_t asset_id);

/**
 * Invalidate all cache slots. Pinned slots are NOT flushed.
 * Use cache_flush_all() to force-flush everything including pinned slots.
 */
void cache_flush(asset_cache_t *cache);

/**
 * Force-flush ALL cache slots including pinned ones. Resets all pin counts.
 */
void cache_flush_all(asset_cache_t *cache);

/**
 * Get cache hit rate as a percentage (0-100).
 */
int cache_hit_rate(asset_cache_t *cache);

#endif
