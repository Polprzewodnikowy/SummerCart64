#include "ff.h"
#include "diskio.h"
#include "../io.h"
#include "../sc64.h"
#include "../error.h"

DSTATUS status = STA_NOINIT;


DSTATUS disk_status (BYTE pdrv) {
    if (pdrv > 0) {
        return STA_NODISK;
    }
    return status;
}

DSTATUS disk_initialize (BYTE pdrv) {
    if (pdrv > 0) {
        return STA_NODISK;
    }
    if (!sc64_sd_card_initialize()) {
        status &= ~(STA_NOINIT);
    }
    return status;
}

DRESULT disk_read (BYTE pdrv, BYTE *buff, LBA_t sector, UINT count) {
    if (pdrv > 0) {
        return RES_PARERR;
    }
    uint32_t physical_address = ((uint32_t) (buff)) & 0x1FFFFFFF;
    uint32_t block = 0;
    while (count > 0) {
        if (physical_address < (8 * 1024 * 1024)) {
            block = ((count > 16) ? 16 : count);
            if (sc64_sd_read_sectors(sector, 0x1FFE0000UL, block * 512)) {
                return RES_ERROR;
            }
            for (uint32_t i = 0; i < (block * 512); i += 4) {
                uint32_t data = pi_io_read((uint32_t *) (0x1FFE0000UL + i));
                uint8_t *ptr = (uint8_t *) (&data);
                for (int j = 0; j < 4; j++) {
                    *buff++ = *ptr++;
                }
            }
        } else {
            block = ((count > 256) ? 256 : count);
            if (sc64_sd_read_sectors(sector, physical_address, block * 512)) {
                error_display("Kurwa 0x%08X, 0x%08X\n", physical_address, block);
                return RES_ERROR;
            }
            physical_address += (block * 512);
        }
        count -= block;
        sector += block;
    }
    return RES_OK;
}

#if !FF_FS_READONLY
DRESULT disk_write (BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count) {
    // if (pdrv >= __DRIVE_COUNT) {
        // return RES_PARERR;
    // }
    // if (sc64_storage_write((drive_id_t) (pdrv), buff, sector, count)) {
        return RES_ERROR;
    // }
    // return RES_OK;
}
#endif

DRESULT disk_ioctl (BYTE pdrv, BYTE cmd, void *buff) {
    if (cmd == CTRL_SYNC) {
        return RES_OK;
    }
    return RES_PARERR;
}

static uint8_t from_bcd (uint8_t bcd) {
    return ((((bcd >> 4) & 0x0F) * 10) + (bcd & 0x0F));
}

DWORD get_fattime(void) {
    rtc_time_t t;
    sc64_get_time(&t);
    return (
        ((from_bcd(t.year) + 20) << 25) |
        (from_bcd(t.month) << 21) |
        (from_bcd(t.day) << 16) |
        (from_bcd(t.hour) << 11) |
        (from_bcd(t.minute) << 5) |
        (from_bcd(t.second) >> 1)
    );
}
