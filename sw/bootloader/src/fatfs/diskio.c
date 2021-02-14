#include "ff.h"
#include "diskio.h"

#include "sc64_sd.h"


DSTATUS disk_status(BYTE pdrv) {
    if (pdrv > 0) {
        return STA_NOINIT;
    }

    return sc64_sd_status_get() ? 0 : STA_NOINIT;
}

DSTATUS disk_initialize(BYTE pdrv) {
    if (pdrv > 0) {
        return STA_NOINIT;
    }

    if (!sc64_sd_status_get()) {
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

    if (pdrv > 0) {
        return RES_PARERR;
    }

    error = sc64_sd_sectors_read(sector, count, buff);

    if (error != E_OK) {
        switch (error) {
            case E_NO_INIT:
                return RES_NOTRDY;
            case E_PAR_ERROR:
                return RES_PARERR;
            default:
                return RES_ERROR;
        }
    }

    return RES_OK;
}

#if !FF_FS_READONLY

DRESULT disk_write (BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count) {
    sc64_sd_err_t error;

    if (pdrv > 0) {
        return RES_PARERR;
    }

    error = sc64_sd_sectors_write(sector, count, (uint8_t *) buff);

    if (error != E_OK) {
        switch (error) {
            case E_NO_INIT:
                return RES_NOTRDY;
            case E_PAR_ERROR:
                return RES_PARERR;
            default:
                return RES_ERROR;
        }
    }

    return RES_OK;
}

#endif

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff) {
    return RES_PARERR;
}
