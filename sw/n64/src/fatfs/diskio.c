#include "ff.h"
#include "diskio.h"
#include "../sc64.h"


DSTATUS disk_status (BYTE pdrv) {
    return 0;
}

DSTATUS disk_initialize (BYTE pdrv) {
    return 0;
}

DRESULT disk_read (BYTE pdrv, BYTE *buff, LBA_t sector, UINT count) {
    if (sc64_storage_read(pdrv, buff, sector, count)) {
        return RES_ERROR;
    }
    return RES_OK;
}

#ifndef FF_FS_READONLY
DRESULT disk_write (BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count) {
    if (sc64_storage_write(pdrv, buff, sector, count)) {
        return RES_ERROR;
    }
    return RES_OK;
}
#endif

DRESULT disk_ioctl (BYTE pdrv, BYTE cmd, void *buff) {
    return RES_PARERR;
}
