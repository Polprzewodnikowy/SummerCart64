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
    if (pdrv == 0) {
        return RES_NOTRDY;
    } else if (pdrv == 1) {
        sc64_debug_fsd_read(buff, sector, count);
        return RES_OK;
    }
    return RES_PARERR;
}

#if FF_FS_READONLY == 0
DRESULT disk_write (BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count) {
    if (pdrv == 0) {
        return RES_NOTRDY;
    } else if (pdrv == 1) {
        sc64_debug_fsd_write(buff, sector, count);
        return RES_OK;
    }
    return RES_PARERR;
}
#endif

DRESULT disk_ioctl (BYTE pdrv, BYTE cmd, void *buff) {
    return RES_PARERR;
}
