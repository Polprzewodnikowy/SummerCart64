#include "ff.h"
#include "diskio.h"
#include "../sc64.h"


DSTATUS status[__DRIVE_COUNT] = { STA_NOINIT, STA_NOINIT };


DSTATUS disk_status (BYTE pdrv) {
    if (pdrv >= __DRIVE_COUNT) {
        return STA_NODISK;
    }
    return status[pdrv];
}

DSTATUS disk_initialize (BYTE pdrv) {
    if (pdrv >= __DRIVE_COUNT) {
        return STA_NODISK;
    }
    if (!sc64_storage_init((drive_id_t) (pdrv))) {
        status[pdrv] &= ~(STA_NOINIT);
    }
    return status[pdrv];
}

DRESULT disk_read (BYTE pdrv, BYTE *buff, LBA_t sector, UINT count) {
    if (pdrv >= __DRIVE_COUNT) {
        return RES_PARERR;
    }
    if (sc64_storage_read((drive_id_t) (pdrv), buff, sector, count)) {
        return RES_ERROR;
    }
    return RES_OK;
}

#if !FF_FS_READONLY
DRESULT disk_write (BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count) {
    if (pdrv >= __DRIVE_COUNT) {
        return RES_PARERR;
    }
    if (sc64_storage_write((drive_id_t) (pdrv), buff, sector, count)) {
        return RES_ERROR;
    }
    return RES_OK;
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
