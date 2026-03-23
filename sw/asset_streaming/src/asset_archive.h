/**
 * SC64 Asset Archive Format
 *
 * On-disk format for sector-aligned, LZ4-compressed asset archives.
 * Designed for streaming from SD card on SummerCart64.
 *
 * Archive layout on SD card (all sector-aligned):
 *
 *   [Header]              1 sector (512 bytes)
 *   [Index Table]         N sectors (ceil(asset_count * 16 / 512))
 *   [Asset Data 0]        sector-aligned, compressed
 *   [Asset Data 1]        sector-aligned, compressed
 *   ...
 *
 * Each asset is stored LZ4-compressed and padded to a sector boundary.
 * The index table maps asset IDs to their location and size on disk.
 */

#ifndef ASSET_ARCHIVE_H__
#define ASSET_ARCHIVE_H__

#include <stdint.h>

#define ASSET_ARCHIVE_MAGIC         (0x53433634)    /* "SC64" */
#define ASSET_ARCHIVE_VERSION       (1)
#define ASSET_SECTOR_SIZE           (512)

/* Flags for asset_entry_t */
#define ASSET_FLAG_COMPRESSED       (1 << 0)        /* LZ4 compressed */
#define ASSET_FLAG_RESIDENT         (1 << 1)        /* Hint: keep in resident memory */
#define ASSET_FLAG_PREFETCH         (1 << 2)        /* Hint: prefetch when neighbors load */

/* Archive header - occupies exactly 1 sector (512 bytes) */
typedef struct __attribute__((packed, aligned(4))) {
    uint32_t magic;                 /* ASSET_ARCHIVE_MAGIC */
    uint32_t version;               /* ASSET_ARCHIVE_VERSION */
    uint32_t asset_count;           /* Number of assets in archive */
    uint32_t index_sector;          /* Sector offset of index table (relative to archive base) */
    uint32_t index_sector_count;    /* Number of sectors the index table occupies */
    uint32_t total_sectors;         /* Total sectors in the archive */
    uint8_t reserved[512 - 24];     /* Pad to exactly 1 sector */
} asset_archive_header_t;

_Static_assert(sizeof(asset_archive_header_t) == ASSET_SECTOR_SIZE,
    "Archive header must be exactly 1 sector");

/* Index entry - 16 bytes per asset, tightly packed */
typedef struct __attribute__((packed, aligned(4))) {
    uint32_t asset_id;              /* Unique asset identifier */
    uint32_t sector_offset;         /* Sector offset of data (relative to archive base) */
    uint16_t sector_count;          /* Number of sectors occupied (compressed + padding) */
    uint16_t flags;                 /* ASSET_FLAG_* */
    uint32_t decompressed_size;     /* Size after decompression (or raw size if uncompressed) */
} asset_entry_t;

_Static_assert(sizeof(asset_entry_t) == 16,
    "Index entry must be exactly 16 bytes");

/* Maximum assets per archive (practical limit for N64 RAM index table) */
#define ASSET_MAX_ENTRIES           (8192)

/* Maximum compressed size per asset: 64 sectors * 512 = 32 KiB per sector_count unit.
 * With uint16_t sector_count, max = 65535 * 512 = ~32 MB per asset. */

#endif
