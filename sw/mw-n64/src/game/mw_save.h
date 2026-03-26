/*
 * MW-N64: Save system — 1Mbit (128 KB) FlashRAM
 *
 * The N64's FlashRAM provides 128 KB of non-volatile storage.
 * Morrowind save data needs to capture:
 *   - Player character state (attributes, skills, inventory, position)
 *   - Quest/journal progress (script globals, journal indices)
 *   - Modified world state (moved/disabled objects, dead NPCs)
 *   - Active effects and timers
 *
 * 128 KB budget allocation:
 *   Header + checksum:         256 bytes
 *   Player character:        4,096 bytes
 *   Inventory (256 items):   8,192 bytes
 *   Quest journal (512 ent): 4,096 bytes
 *   Global variables (256):  4,096 bytes
 *   Script locals:           8,192 bytes
 *   Modified refs (1024):   32,768 bytes
 *   Dead NPCs/creatures:     4,096 bytes
 *   Faction standings:        2,048 bytes
 *   Active effects:           2,048 bytes
 *   Reserve:                 61,440 bytes
 *   ─────────────────────────────────
 *   Total:                  131,072 bytes (128 KB)
 *
 * 3 save slots = 128 KB / 3 ≈ 42 KB per slot.
 * Alternatively: 1 active + 1 backup = 64 KB each (more data per save).
 *
 * FlashRAM access: 128 bytes per write (page size), 128 KB total.
 * Write time: ~100ms per page. Full save: ~100 pages = ~10 seconds.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MW_SAVE_H
#define MW_SAVE_H

#include "mw_character.h"
#include <stdint.h>
#include <stdbool.h>

/* -------------------------------------------------------------------
 * FlashRAM hardware interface
 * ------------------------------------------------------------------- */

#define FLASH_BASE          0x08000000  /* PI address for FlashRAM */
#define FLASH_PAGE_SIZE     128         /* Bytes per write page */
#define FLASH_TOTAL_SIZE    (128 * 1024) /* 128 KB = 1 Mbit */

/* FlashRAM commands (written to 0x08010000) */
#define FLASH_CMD_STATUS    0xD2000000
#define FLASH_CMD_READ      0xF0000000
#define FLASH_CMD_ERASE     0x78000000
#define FLASH_CMD_WRITE     0xA5000000
#define FLASH_CMD_EXECUTE   0xD2000000

/* -------------------------------------------------------------------
 * Save slot layout
 * ------------------------------------------------------------------- */

#define SAVE_SLOTS          2       /* 2 slots × 64 KB each */
#define SAVE_SLOT_SIZE      (FLASH_TOTAL_SIZE / SAVE_SLOTS)
#define SAVE_MAGIC          0x4D574E36  /* "MWN6" */
#define SAVE_VERSION        1

/* Modified world reference (moved/disabled/dead entities) */
typedef struct {
    uint32_t ref_num;       /* ESM reference number */
    uint8_t  flags;         /* 0x01=disabled, 0x02=dead, 0x04=moved */
    float    new_x, new_y, new_z;  /* New position (if moved) */
} save_mod_ref_t;

/* Journal entry */
typedef struct {
    char     quest_id[32];
    int16_t  index;         /* Journal index (quest progress stage) */
} save_journal_t;

/* Save file header */
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t checksum;      /* CRC32 of data after header */
    uint32_t play_time;     /* Total play time in seconds */
    char     player_name[32];
    int32_t  player_level;
    char     location[64];  /* Current cell name */
    uint32_t timestamp;     /* Save creation time (frame count) */
} save_header_t;

/* Complete save data structure */
typedef struct {
    save_header_t header;

    /* Player */
    mw_player_t player;

    /* Quest progress */
    save_journal_t journal[512];
    int journal_count;

    /* Global variables (script state) */
    struct {
        char  name[32];
        float value;
    } globals[256];
    int global_count;

    /* Modified world objects */
    save_mod_ref_t modified_refs[1024];
    int modified_ref_count;
} save_data_t;

/* -------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------- */

/* Initialize FlashRAM hardware */
void save_init(void);

/* Check if a save slot has valid data */
bool save_slot_valid(int slot);

/* Get header from a save slot (for save/load menu display) */
bool save_read_header(int slot, save_header_t *header);

/* Save game to a slot */
bool save_write(int slot, const save_data_t *data);

/* Load game from a slot */
bool save_read(int slot, save_data_t *data);

/* Erase a save slot */
bool save_erase(int slot);

/* Calculate checksum */
uint32_t save_checksum(const void *data, uint32_t size);

#endif /* MW_SAVE_H */
