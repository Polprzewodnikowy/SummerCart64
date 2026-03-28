/**
 * SC64 Asset Streaming Manager
 *
 * Ties together SD streaming, LZ4 decompression, and LRU caching
 * to provide a simple asset_load(id) -> pointer API.
 */

#include "asset_manager.h"
#include "asset_debug.h"
#include "lz4_decode.h"
#include <string.h>

/* -------------------------------------------------------------------
 * SD backend dispatch
 *
 * When a backend is provided, all SD operations go through it.
 * When backend is NULL (production on real N64), use SC64 hardware.
 * ------------------------------------------------------------------- */

/* Forward declarations for backend dispatch */
static uint32_t sd_init_dispatch(asset_manager_t *mgr);
static uint32_t sd_deinit_dispatch(asset_manager_t *mgr);
static uint32_t sd_read_dispatch(asset_manager_t *mgr, void *dest,
                                  uint32_t sector, uint32_t count);

#ifndef ASSET_HOST_BUILD
/* -------------------------------------------------------------------
 * SC64 hardware interface (only compiled for N64 target)
 * ------------------------------------------------------------------- */

typedef volatile uint32_t io32_t;

/* All hardware registers must be accessed via KSEG1 uncached addresses.
 * Physical address | 0xA0000000 = KSEG1 uncached virtual address.
 * Without this, accesses cause TLB miss exceptions. */
#define UNCACHED(addr)      ((addr) | 0xA0000000UL)

/* Cache operations for DMA coherency */
static inline void data_cache_hit_writeback(void *addr, uint32_t len) {
    uint32_t a = (uint32_t)addr & ~0xF;
    uint32_t end = ((uint32_t)addr + len + 0xF) & ~0xF;
    while (a < end) {
        asm volatile ("cache 0x19, 0(%0)" :: "r"(a));  /* D-cache hit writeback */
        a += 16;
    }
}

static inline void data_cache_hit_invalidate(void *addr, uint32_t len) {
    uint32_t a = (uint32_t)addr & ~0xF;
    uint32_t end = ((uint32_t)addr + len + 0xF) & ~0xF;
    while (a < end) {
        asm volatile ("cache 0x11, 0(%0)" :: "r"(a));  /* D-cache hit invalidate */
        a += 16;
    }
}

#define SC64_REGS_BASE      UNCACHED(0x1FFF0000UL)
#define SC64_REG_SCR        (*(io32_t *)(SC64_REGS_BASE + 0x00))
#define SC64_REG_DATA0      (*(io32_t *)(SC64_REGS_BASE + 0x04))
#define SC64_REG_DATA1      (*(io32_t *)(SC64_REGS_BASE + 0x08))
#define SC64_REG_KEY        (*(io32_t *)(SC64_REGS_BASE + 0x10))

#define SC64_SCR_CPU_BUSY   (1U << 31)
#define SC64_SCR_CMD_ERROR  (1U << 30)

#define PI_BASE             UNCACHED(0x04600000UL)
#define PI_REG_SR           (*(io32_t *)(PI_BASE + 0x10))
#define PI_SR_DMA_BUSY      (1 << 0)
#define PI_SR_IO_BUSY       (1 << 1)

static inline void pi_wait(void) {
    while (PI_REG_SR & (PI_SR_DMA_BUSY | PI_SR_IO_BUSY));
}

static inline uint32_t pi_read(io32_t *addr) {
    pi_wait();
    uint32_t val = *addr;
    pi_wait();
    return val;
}

static inline void pi_write(io32_t *addr, uint32_t val) {
    pi_wait();
    *addr = val;
    pi_wait();
}

static uint32_t sc64_cmd(uint8_t cmd_id, uint32_t arg0, uint32_t arg1,
                         uint32_t *rsp0, uint32_t *rsp1) {
    pi_write(&SC64_REG_DATA0, arg0);
    pi_write(&SC64_REG_DATA1, arg1);
    pi_write(&SC64_REG_SCR, cmd_id);

    uint32_t sr;
    do {
        sr = pi_read(&SC64_REG_SCR);
    } while (sr & SC64_SCR_CPU_BUSY);

    if (sr & SC64_SCR_CMD_ERROR) {
        return pi_read(&SC64_REG_DATA0);
    }

    if (rsp0) *rsp0 = pi_read(&SC64_REG_DATA0);
    if (rsp1) *rsp1 = pi_read(&SC64_REG_DATA1);
    return 0;
}

#define CMD_SD_CARD_OP      ('i')
#define CMD_SD_SECTOR_SET   ('I')
#define CMD_SD_READ         ('s')
#define SD_OP_INIT          (1)
#define SD_OP_DEINIT        (0)

static uint32_t sc64_sd_init(void) {
    return sc64_cmd(CMD_SD_CARD_OP, 0, SD_OP_INIT, NULL, NULL);
}

static uint32_t sc64_sd_deinit(void) {
    return sc64_cmd(CMD_SD_CARD_OP, 0, SD_OP_DEINIT, NULL, NULL);
}

/*
 * SC64 SD read: two-step process (from bootloader's diskio.c):
 *   1. SD card → SC64 SDRAM buffer (via SC64 command)
 *   2. SC64 SDRAM buffer → RDRAM (via PI DMA)
 *
 * The SC64 SD read command reads into SC64's own SDRAM at 0x1FFE0000,
 * NOT directly into RDRAM. We then PI DMA from that buffer into our
 * destination.
 */
#define SC64_BUFFER_BASE    (0x1FFE0000UL)  /* Physical PI address of SC64 SDRAM buffer */
#define SC64_BUFFER_SIZE    (8192)         /* 8 KB buffer (from sc64.h sc64_buffers_t) */

#define PI_REG_DRAM_ADDR    (*(io32_t *)(UNCACHED(0x04600000UL) + 0x00))
#define PI_REG_CART_ADDR    (*(io32_t *)(UNCACHED(0x04600000UL) + 0x04))
#define PI_REG_WR_LEN       (*(io32_t *)(UNCACHED(0x04600000UL) + 0x0C))

static uint32_t sc64_sd_read_sectors(void *dest, uint32_t sector, uint32_t count) {
    uint32_t err;
    uint8_t *dst = (uint8_t *)dest;
    uint32_t remaining = count;
    uint32_t current_sector = sector;

    while (remaining > 0) {
        /* Read up to buffer-size sectors at a time */
        uint32_t chunk = remaining;
        uint32_t max_sectors = SC64_BUFFER_SIZE / 512;
        if (chunk > max_sectors) chunk = max_sectors;

        /* Step 1: SD → SC64 SDRAM buffer (pass physical PI address) */
        err = sc64_cmd(CMD_SD_SECTOR_SET, current_sector, 0, NULL, NULL);
        if (err) return err;
        err = sc64_cmd(CMD_SD_READ, SC64_BUFFER_BASE, chunk, NULL, NULL);
        if (err) return err;

        /* Step 2: PI DMA from SC64 SDRAM → RDRAM
         * PI_DRAM_ADDR = physical RDRAM address (strip KSEG bits)
         * PI_CART_ADDR = PI bus address of SC64 buffer (0x1FFE0000)
         * PI_WR_LEN = bytes - 1 (triggers cart→RDRAM DMA) */
        uint32_t bytes = chunk * 512;
        data_cache_hit_invalidate(dst, bytes);
        pi_wait();
        PI_REG_DRAM_ADDR = (uint32_t)dst & 0x1FFFFFFF;
        PI_REG_CART_ADDR = SC64_BUFFER_BASE;
        PI_REG_WR_LEN = bytes - 1;
        pi_wait();

        dst += bytes;
        current_sector += chunk;
        remaining -= chunk;
    }

    return 0;
}

#endif /* !ASSET_HOST_BUILD */

/* Backend dispatch: routes to backend or SC64 hardware */
static uint32_t sd_init_dispatch(asset_manager_t *mgr) {
    if (mgr->backend) return mgr->backend->init(mgr->backend->ctx);
#ifndef ASSET_HOST_BUILD
    return sc64_sd_init();
#else
    return 1; /* No backend and no hardware */
#endif
}

static uint32_t sd_deinit_dispatch(asset_manager_t *mgr) {
    if (mgr->backend) return mgr->backend->deinit(mgr->backend->ctx);
#ifndef ASSET_HOST_BUILD
    return sc64_sd_deinit();
#else
    return 0;
#endif
}

static uint32_t sd_read_dispatch(asset_manager_t *mgr, void *dest,
                                  uint32_t sector, uint32_t count) {
    if (mgr->backend) return mgr->backend->read_sectors(mgr->backend->ctx, dest, sector, count);
#ifndef ASSET_HOST_BUILD
    return sc64_sd_read_sectors(dest, sector, count);
#else
    return 1; /* No backend and no hardware */
#endif
}

/* -------------------------------------------------------------------
 * Index lookup (binary search, index must be sorted by asset_id)
 * ------------------------------------------------------------------- */

static const asset_entry_t *index_find(const asset_manager_t *mgr, uint32_t asset_id) {
    /* Linear scan - simple and fine for <8K entries at load time */
    for (uint32_t i = 0; i < mgr->index_count; i++) {
        if (mgr->index[i].asset_id == asset_id) {
            return &mgr->index[i];
        }
    }
    return NULL;
}

/* -------------------------------------------------------------------
 * Core load routine
 * ------------------------------------------------------------------- */

static void *load_asset_into_slot(asset_manager_t *mgr, const asset_entry_t *entry,
                                   int slot_idx) {
    uint8_t *slot_data = (uint8_t *)cache_slot_data(&mgr->cache, slot_idx);
    uint32_t sd_sector = mgr->archive_base_sector + entry->sector_offset;
    uint32_t sector_bytes = (uint32_t)entry->sector_count * ASSET_SECTOR_SIZE;

    if (entry->flags & ASSET_FLAG_COMPRESSED) {
        /* Read compressed data into staging buffer */
        if (sector_bytes > mgr->staging_size) {
            mgr->last_error = ASSET_ERR_TOO_LARGE;
            return NULL;
        }

        uint32_t err = sd_read_dispatch(mgr, mgr->staging_buffer, sd_sector, entry->sector_count);
        if (err) {
            mgr->last_error = ASSET_ERR_SD_READ;
            return NULL;
        }

        /* Decompress into cache slot.
         * Use exact compressed_size (not sector-padded size) so the
         * decoder doesn't try to parse padding bytes as LZ4 tokens. */
        int decoded = lz4_decode_block(
            mgr->staging_buffer, slot_data,
            (int)entry->compressed_size, (int)mgr->cache.config.slot_size
        );
        if (decoded < 0 || (uint32_t)decoded != entry->decompressed_size) {
            mgr->last_error = ASSET_ERR_DECOMPRESS;
            return NULL;
        }

        cache_slot_ready(&mgr->cache, slot_idx, entry->decompressed_size);
    } else {
        /* Uncompressed: read directly into cache slot */
        if (entry->decompressed_size > mgr->cache.config.slot_size) {
            mgr->last_error = ASSET_ERR_TOO_LARGE;
            return NULL;
        }

        uint32_t err = sd_read_dispatch(mgr, slot_data, sd_sector, entry->sector_count);
        if (err) {
            mgr->last_error = ASSET_ERR_SD_READ;
            return NULL;
        }

        cache_slot_ready(&mgr->cache, slot_idx, entry->decompressed_size);
    }

    return slot_data;
}

/* -------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------- */

asset_error_t asset_init(asset_manager_t *mgr, const asset_init_config_t *config) {
    memset(mgr, 0, sizeof(*mgr));
    mgr->archive_base_sector = config->archive_base_sector;
    mgr->staging_buffer = config->staging_buffer;
    mgr->staging_size = config->staging_size;
    mgr->backend = config->backend;

    /* Initialize SD card */
    if (sd_init_dispatch(mgr)) {
        mgr->last_error = ASSET_ERR_SD_INIT;
        return ASSET_ERR_SD_INIT;
    }

    /* Read archive header (1 sector) into staging buffer */
    if (sd_read_dispatch(mgr, config->staging_buffer, config->archive_base_sector, 1)) {
        mgr->last_error = ASSET_ERR_SD_READ;
        return ASSET_ERR_SD_READ;
    }
    memcpy(&mgr->header, config->staging_buffer, sizeof(asset_archive_header_t));

    if (mgr->header.magic != ASSET_ARCHIVE_MAGIC) {
        mgr->last_error = ASSET_ERR_BAD_ARCHIVE;
        return ASSET_ERR_BAD_ARCHIVE;
    }
    if (mgr->header.version != ASSET_ARCHIVE_VERSION) {
        mgr->last_error = ASSET_ERR_VERSION;
        return ASSET_ERR_VERSION;
    }
    if (mgr->header.asset_count > ASSET_MAX_ENTRIES) {
        mgr->last_error = ASSET_ERR_TOO_MANY_ASSETS;
        return ASSET_ERR_TOO_MANY_ASSETS;
    }

    /* Read index table into staging buffer, then copy to permanent RDRAM location.
     * The index lives at the END of the staging buffer to avoid conflicts during
     * normal asset loading (which uses the start of staging). */
    uint32_t index_bytes = mgr->header.asset_count * sizeof(asset_entry_t);
    uint32_t index_sd_sector = config->archive_base_sector + mgr->header.index_sector;

    if (sd_read_dispatch(mgr, config->staging_buffer, index_sd_sector,
                        mgr->header.index_sector_count)) {
        mgr->last_error = ASSET_ERR_SD_READ;
        return ASSET_ERR_SD_READ;
    }

    /* Place index table at end of staging buffer permanently.
     * This means staging_size should be larger than the biggest compressed
     * asset PLUS the index table. For 8192 assets: 8192 * 16 = 128 KB. */
    uint32_t index_offset = config->staging_size - index_bytes;
    index_offset &= ~3;  /* Align to 4 bytes */
    mgr->index = (asset_entry_t *)(config->staging_buffer + index_offset);
    memcpy(mgr->index, config->staging_buffer, index_bytes);
    mgr->index_count = mgr->header.asset_count;

    /* Reduce effective staging size to not overlap index */
    mgr->staging_size = index_offset;

    /* Initialize LRU cache */
    uint32_t slot_count = config->cache_size / config->slot_size;
    if (slot_count > ASSET_MAX_SLOTS) slot_count = ASSET_MAX_SLOTS;

    cache_init(&mgr->cache, mgr->slot_storage,
               config->cache_base, slot_count * config->slot_size,
               config->slot_size);

    mgr->initialized = true;
    return ASSET_OK;
}

void *asset_load(asset_manager_t *mgr, uint32_t asset_id) {
    if (!mgr->initialized) {
        mgr->last_error = ASSET_ERR_NOT_INITIALIZED;
        return NULL;
    }

    /* Try cache first */
    void *cached = cache_lookup(&mgr->cache, asset_id);
    if (cached) {
        ASSET_DBG_ON_LOAD(asset_id, 1, -1);
        return cached;
    }

    /* Cache miss - find in index */
    const asset_entry_t *entry = index_find(mgr, asset_id);
    if (!entry) {
        mgr->last_error = ASSET_ERR_NOT_FOUND;
        return NULL;
    }

    /* Check if asset fits in a slot */
    if (entry->decompressed_size > mgr->cache.config.slot_size) {
        mgr->last_error = ASSET_ERR_TOO_LARGE;
        return NULL;
    }

    /* Allocate a cache slot (may evict LRU) */
    bool evicted;
    int slot = cache_allocate(&mgr->cache, asset_id, &evicted);
    if (slot < 0) {
        mgr->last_error = ASSET_ERR_CACHE_FULL;
        return NULL;
    }

    /* If already cached (allocate found it), return directly */
    if (!evicted && mgr->cache.slots[slot].state == SLOT_READY) {
        return cache_slot_data(&mgr->cache, slot);
    }

    /* Load from SD into slot */
    ASSET_DBG_ON_LOAD(asset_id, 0, slot);
    return load_asset_into_slot(mgr, entry, slot);
}

void *asset_load_with_size(asset_manager_t *mgr, uint32_t asset_id, uint32_t *out_size) {
    void *data = asset_load(mgr, asset_id);
    if (data && out_size) {
        /* Find the slot to get size */
        for (uint32_t i = 0; i < mgr->cache.config.slot_count; i++) {
            if (mgr->cache.slots[i].asset_id == asset_id &&
                mgr->cache.slots[i].state == SLOT_READY) {
                *out_size = mgr->cache.slots[i].data_size;
                break;
            }
        }
    }
    return data;
}

asset_error_t asset_prefetch(asset_manager_t *mgr, uint32_t asset_id) {
    /* Currently just a blocking load. Future: async DMA. */
    void *result = asset_load(mgr, asset_id);
    return result ? ASSET_OK : mgr->last_error;
}

bool asset_pin(asset_manager_t *mgr, uint32_t asset_id) {
    if (!mgr->initialized) return false;
    return cache_pin(&mgr->cache, asset_id);
}

bool asset_unpin(asset_manager_t *mgr, uint32_t asset_id) {
    if (!mgr->initialized) return false;
    return cache_unpin(&mgr->cache, asset_id);
}

void *asset_load_pinned(asset_manager_t *mgr, uint32_t asset_id) {
    void *data = asset_load(mgr, asset_id);
    if (data) {
        cache_pin(&mgr->cache, asset_id);
    }
    return data;
}

bool asset_is_pinned(asset_manager_t *mgr, uint32_t asset_id) {
    if (!mgr->initialized) return false;
    return cache_is_pinned(&mgr->cache, asset_id);
}

bool asset_is_cached(asset_manager_t *mgr, uint32_t asset_id) {
    if (!mgr->initialized) return false;
    return cache_lookup(&mgr->cache, asset_id) != NULL;
}

void asset_flush(asset_manager_t *mgr) {
    if (mgr->initialized) {
        cache_flush(&mgr->cache);
    }
}

void asset_shutdown(asset_manager_t *mgr) {
    if (mgr->initialized) {
        /* In debug mode, check for leaked pins before shutting down */
        asset_debug_check_leaks(mgr);
        sd_deinit_dispatch(mgr);
        mgr->initialized = false;
    }
}

/* -------------------------------------------------------------------
 * Handle API
 * ------------------------------------------------------------------- */

asset_handle_t asset_acquire(asset_manager_t *mgr, uint32_t asset_id) {
    void *data = asset_load_pinned(mgr, asset_id);
    if (!data) return ASSET_HANDLE_INVALID;

    /* Find the slot to capture generation */
    for (uint32_t i = 0; i < mgr->cache.config.slot_count; i++) {
        if (mgr->cache.slots[i].asset_id == asset_id &&
            mgr->cache.slots[i].state == SLOT_READY) {
            return (asset_handle_t){
                .slot = (int16_t)i,
                .generation = (uint16_t)mgr->cache.slots[i].generation,
                .asset_id = asset_id,
            };
        }
    }

    return ASSET_HANDLE_INVALID;
}

void *asset_resolve(asset_manager_t *mgr, asset_handle_t handle) {
    if (handle.slot < 0 || (uint32_t)handle.slot >= mgr->cache.config.slot_count)
        return NULL;

    cache_slot_t *slot = &mgr->cache.slots[handle.slot];
    if (slot->state != SLOT_READY ||
        slot->asset_id != handle.asset_id ||
        (uint16_t)slot->generation != handle.generation) {
        ASSET_DBG_LOG("[ASSET_DBG] STALE HANDLE: asset %u slot %d "
                      "(gen %u != %u)\n",
                      handle.asset_id, handle.slot,
                      handle.generation, (uint16_t)slot->generation);
        return NULL;
    }

    slot->lru_counter = ++mgr->cache.lru_clock;
    return slot->data;
}

void *asset_resolve_with_size(asset_manager_t *mgr, asset_handle_t handle,
                               uint32_t *out_size) {
    void *data = asset_resolve(mgr, handle);
    if (data && out_size) {
        *out_size = mgr->cache.slots[handle.slot].data_size;
    }
    return data;
}

void asset_release(asset_manager_t *mgr, asset_handle_t handle) {
    if (handle.slot < 0) return;
    asset_unpin(mgr, handle.asset_id);
}

const char *asset_error_string(asset_error_t error) {
    switch (error) {
        case ASSET_OK:                  return "OK";
        case ASSET_ERR_SD_INIT:         return "SD card init failed";
        case ASSET_ERR_SD_READ:         return "SD card read error";
        case ASSET_ERR_BAD_ARCHIVE:     return "Invalid archive (bad magic)";
        case ASSET_ERR_VERSION:         return "Unsupported archive version";
        case ASSET_ERR_TOO_MANY_ASSETS: return "Too many assets in archive";
        case ASSET_ERR_NOT_FOUND:       return "Asset not found";
        case ASSET_ERR_DECOMPRESS:      return "LZ4 decompression failed";
        case ASSET_ERR_TOO_LARGE:       return "Asset too large for slot";
        case ASSET_ERR_CACHE_FULL:      return "Cache full (all slots loading)";
        case ASSET_ERR_NOT_INITIALIZED: return "Not initialized";
        default:                        return "Unknown error";
    }
}
