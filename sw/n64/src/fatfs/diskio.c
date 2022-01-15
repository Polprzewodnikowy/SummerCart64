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
    sc64_storage_type_t storage_type;

    if (pdrv == 0) {
        storage_type = SC64_STORAGE_TYPE_SD;
    } else if (pdrv == 1) {
        storage_type = SC64_STORAGE_TYPE_USB;
    } else {
        return RES_PARERR;
    }

    if (sc64_storage_read(storage_type, buff, sector, count) != SC64_STORAGE_OK) {
        return RES_ERROR;
    }

    return RES_OK;
}

DRESULT disk_ioctl (BYTE pdrv, BYTE cmd, void *buff) {
    return RES_PARERR;
}
