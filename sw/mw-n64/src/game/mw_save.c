/*
 * MW-N64: FlashRAM save system — implementation
 *
 * N64 FlashRAM (MX29L1100/MX29L1101):
 *   - 128 KB total (1 Mbit)
 *   - 128-byte page writes
 *   - Sector erase (128 pages = 16 KB per sector, 8 sectors)
 *   - Read: via PI DMA
 *   - Write: command sequence → page program → verify
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "mw_save.h"
#include <string.h>

/* PI registers for FlashRAM access */
#define PI_BASE_REG     0xA4600000
#define PI_DRAM_ADDR    (*(volatile uint32_t *)(PI_BASE_REG + 0x00))
#define PI_CART_ADDR    (*(volatile uint32_t *)(PI_BASE_REG + 0x04))
#define PI_RD_LEN       (*(volatile uint32_t *)(PI_BASE_REG + 0x08))
#define PI_WR_LEN       (*(volatile uint32_t *)(PI_BASE_REG + 0x0C))
#define PI_STATUS       (*(volatile uint32_t *)(PI_BASE_REG + 0x10))

#define PI_STATUS_BUSY  0x03

static void pi_wait(void) {
    while (PI_STATUS & PI_STATUS_BUSY);
}

/* Write a command to FlashRAM command register */
static void flash_cmd(uint32_t cmd) {
    pi_wait();
    *(volatile uint32_t *)0xA8010000 = cmd;
    pi_wait();
}

/* -------------------------------------------------------------------
 * FlashRAM read: DMA from cart space to RDRAM
 * ------------------------------------------------------------------- */

static void flash_read(uint32_t flash_offset, void *dest, uint32_t size) {
    flash_cmd(FLASH_CMD_READ);

    /* DMA from FlashRAM to RDRAM */
    uint32_t aligned_size = (size + 1) & ~1;  /* Round up to 2-byte */
    pi_wait();
    PI_DRAM_ADDR = (uint32_t)(uintptr_t)dest & 0x1FFFFFFF;
    PI_CART_ADDR = FLASH_BASE + flash_offset;
    PI_WR_LEN = aligned_size - 1;
    pi_wait();
}

/* -------------------------------------------------------------------
 * FlashRAM write: erase sector, then write pages
 * ------------------------------------------------------------------- */

static void flash_erase_sector(uint32_t sector) {
    /* Each sector = 16 KB (128 pages × 128 bytes) */
    flash_cmd(FLASH_CMD_ERASE);
    pi_wait();
    *(volatile uint32_t *)(0xA8000000 + sector * 128) = 0;
    pi_wait();
    flash_cmd(FLASH_CMD_EXECUTE);
    pi_wait();
    /* Wait for erase to complete (~200ms) */
}

static void flash_write_page(uint32_t page_num, const void *data) {
    /* Write 128 bytes to a single page */
    flash_cmd(FLASH_CMD_WRITE);

    /* DMA 128 bytes from RDRAM to flash buffer */
    pi_wait();
    PI_DRAM_ADDR = (uint32_t)(uintptr_t)data & 0x1FFFFFFF;
    PI_CART_ADDR = FLASH_BASE;  /* Flash buffer */
    PI_RD_LEN = FLASH_PAGE_SIZE - 1;
    pi_wait();

    /* Program the page */
    *(volatile uint32_t *)(0xA8000000 + page_num * 128) = 0;
    pi_wait();
    flash_cmd(FLASH_CMD_EXECUTE);
    pi_wait();
}

/* -------------------------------------------------------------------
 * CRC32 checksum (same as zlib CRC32, polynomial 0xEDB88320)
 * ------------------------------------------------------------------- */

uint32_t save_checksum(const void *data, uint32_t size) {
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFF;
    for (uint32_t i = 0; i < size; i++) {
        crc ^= p[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    return ~crc;
}

/* -------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------- */

void save_init(void) {
    /* Flash is ready after boot — no special init needed */
    flash_cmd(FLASH_CMD_STATUS);
}

bool save_slot_valid(int slot) {
    if (slot < 0 || slot >= SAVE_SLOTS) return false;

    save_header_t header;
    flash_read(slot * SAVE_SLOT_SIZE, &header, sizeof(header));

    return header.magic == SAVE_MAGIC && header.version == SAVE_VERSION;
}

bool save_read_header(int slot, save_header_t *header) {
    if (slot < 0 || slot >= SAVE_SLOTS) return false;
    flash_read(slot * SAVE_SLOT_SIZE, header, sizeof(*header));
    return header->magic == SAVE_MAGIC;
}

bool save_write(int slot, const save_data_t *data) {
    if (slot < 0 || slot >= SAVE_SLOTS) return false;

    /* Build save buffer */
    uint32_t offset = slot * SAVE_SLOT_SIZE;
    uint32_t data_size = sizeof(save_data_t);
    if (data_size > SAVE_SLOT_SIZE) data_size = SAVE_SLOT_SIZE;

    /* Calculate checksum (skip header's own checksum field) */
    save_data_t save_copy;
    memcpy(&save_copy, data, sizeof(save_copy));
    save_copy.header.magic = SAVE_MAGIC;
    save_copy.header.version = SAVE_VERSION;
    save_copy.header.checksum = 0;
    save_copy.header.checksum = save_checksum(
        ((uint8_t *)&save_copy) + sizeof(save_header_t),
        data_size - sizeof(save_header_t));

    /* Erase sectors covering this slot */
    uint32_t start_sector = (offset / (16 * 1024));
    uint32_t end_sector = ((offset + data_size - 1) / (16 * 1024));
    for (uint32_t s = start_sector; s <= end_sector; s++) {
        flash_erase_sector(s);
    }

    /* Write page by page */
    const uint8_t *src = (const uint8_t *)&save_copy;
    uint32_t remaining = data_size;
    uint32_t page = offset / FLASH_PAGE_SIZE;

    /* Need 128-byte aligned buffer for DMA */
    uint8_t page_buf[FLASH_PAGE_SIZE] __attribute__((aligned(16)));

    while (remaining > 0) {
        uint32_t chunk = remaining < FLASH_PAGE_SIZE ? remaining : FLASH_PAGE_SIZE;
        memset(page_buf, 0xFF, FLASH_PAGE_SIZE);
        memcpy(page_buf, src, chunk);
        flash_write_page(page, page_buf);
        src += chunk;
        remaining -= chunk;
        page++;
    }

    return true;
}

bool save_read(int slot, save_data_t *data) {
    if (slot < 0 || slot >= SAVE_SLOTS) return false;

    flash_read(slot * SAVE_SLOT_SIZE, data, sizeof(*data));

    if (data->header.magic != SAVE_MAGIC) return false;
    if (data->header.version != SAVE_VERSION) return false;

    /* Verify checksum */
    uint32_t stored_crc = data->header.checksum;
    data->header.checksum = 0;
    uint32_t calc_crc = save_checksum(
        ((uint8_t *)data) + sizeof(save_header_t),
        sizeof(*data) - sizeof(save_header_t));
    data->header.checksum = stored_crc;

    return calc_crc == stored_crc;
}

bool save_erase(int slot) {
    if (slot < 0 || slot >= SAVE_SLOTS) return false;

    uint32_t offset = slot * SAVE_SLOT_SIZE;
    uint32_t start_sector = offset / (16 * 1024);
    uint32_t end_sector = (offset + SAVE_SLOT_SIZE - 1) / (16 * 1024);

    for (uint32_t s = start_sector; s <= end_sector; s++) {
        flash_erase_sector(s);
    }
    return true;
}
