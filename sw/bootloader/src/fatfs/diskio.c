#include "ff.h"
#include "diskio.h"

#include "sc64_sd.h"


DSTATUS disk_status(BYTE pdrv) {
    if (pdrv > 0) {
        return STA_NOINIT;
    }

    return sc64_sd_get_status() ? 0 : STA_NOINIT;
}

DSTATUS disk_initialize(BYTE pdrv) {
    if (pdrv > 0) {
        return STA_NOINIT;
    }

    if (!sc64_sd_get_status()) {
        if (sc64_sd_init()) {
            return 0;
        }
    } else {
        return 0;
    }

    return STA_NOINIT;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count) {
    sc64_sd_err_t error;

    if ((pdrv > 0) || (count == 0)) {
        return RES_PARERR;
    }

    if (!sc64_sd_get_status()) {
        return RES_NOTRDY;
    }

    error = sc64_sd_read_sectors(sector, count, buff);

    if (error != E_OK) {
        return RES_ERROR;
    }

    return RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff) {
    return RES_PARERR;
}
