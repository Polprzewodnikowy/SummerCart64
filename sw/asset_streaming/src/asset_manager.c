/**
 * SC64 Asset Streaming Manager
 *
 * Ties together SD streaming, LZ4 decompression, and LRU caching
 * to provide a simple asset_load(id) -> pointer API.
 */

#include "asset_manager.h"
#include "lz4_decode.h"
#include <string.h>

/* -------------------------------------------------------------------
 * SC64 hardware interface (minimal standalone, no bootloader dependency)
 * ------------------------------------------------------------------- */

typedef volatile uint32_t io32_t;

#define SC64_REGS_BASE      (0x1FFF0000UL)
#define SC64_REG_SCR        (*(io32_t *)(SC64_REGS_BASE + 0x00))
#define SC64_REG_DATA0      (*(io32_t *)(SC64_REGS_BASE + 0x04))
#define SC64_REG_DATA1      (*(io32_t *)(SC64_REGS_BASE + 0x08))
#define SC64_REG_KEY        (*(io32_t *)(SC64_REGS_BASE + 0x10))

#define SC64_SCR_CPU_BUSY   (1U << 31)
#define SC64_SCR_CMD_ERROR  (1U << 30)

#define PI_BASE             (0x04600000UL)
#define PI_REG_SR           (*(io32_t *)(PI_BASE + 0x10))
#define PI_SR_DMA_BUSY      (1 << 0)
#define PI_SR_IO_BUSY       (1 << 1)

/* Wait for PI bus to be idle */
static inline void pi_wait(void) {
    while (PI_REG_SR & (PI_SR_DMA_BUSY | PI_SR_IO_BUSY));
}

/* PI I/O read (32-bit, uncached) */
static inline uint32_t pi_read(io32_t *addr) {
    pi_wait();
    uint32_t val = *addr;
    pi_wait();
    return val;
}

/* PI I/O write (32-bit, uncached) */
static inline void pi_write(io32_t *addr, uint32_t val) {
    pi_wait();
    *addr = val;
    pi_wait();
}

/* Execute an SC64 command and wait for completion */
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

/* SC64 command IDs */
#define CMD_SD_CARD_OP      ('i')
#define CMD_SD_SECTOR_SET   ('I')
#define CMD_SD_READ         ('s')

#define SD_OP_INIT          (1)
#define SD_OP_DEINIT        (0)

/* Initialize SD card */
static uint32_t sd_init(void) {
    return sc64_cmd(CMD_SD_CARD_OP, 0, SD_OP_INIT, NULL, NULL);
}

/* Deinitialize SD card */
static uint32_t sd_deinit(void) {
    return sc64_cmd(CMD_SD_CARD_OP, 0, SD_OP_DEINIT, NULL, NULL);
}

/* Read sectors from SD directly to a PI-mapped address */
static uint32_t sd_read_sectors(void *pi_address, uint32_t sector, uint32_t count) {
    uint32_t err;
    err = sc64_cmd(CMD_SD_SECTOR_SET, sector, 0, NULL, NULL);
    if (err) return err;
    err = sc64_cmd(CMD_SD_READ, (uint32_t)pi_address, count, NULL, NULL);
    return err;
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

        uint32_t err = sd_read_sectors(mgr->staging_buffer, sd_sector, entry->sector_count);
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

        uint32_t err = sd_read_sectors(slot_data, sd_sector, entry->sector_count);
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

    /* Initialize SD card */
    if (sd_init()) {
        mgr->last_error = ASSET_ERR_SD_INIT;
        return ASSET_ERR_SD_INIT;
    }

    /* Read archive header (1 sector) into staging buffer */
    if (sd_read_sectors(config->staging_buffer, config->archive_base_sector, 1)) {
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

    if (sd_read_sectors(config->staging_buffer, index_sd_sector,
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
    if (cached) return cached;

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
        sd_deinit();
        mgr->initialized = false;
    }
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
