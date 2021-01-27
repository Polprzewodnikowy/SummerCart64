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
    uint8_t success;
    uint8_t response;

    if ((pdrv > 0) || (count == 0)) {
        return RES_PARERR;
    }

    if (!sc64_sd_get_status()) {
        return RES_NOTRDY;
    }

    if (count == 1) {
        success = sc64_sd_cmd_send(CMD17, sector, &response);

        if (!success || response) {
            return RES_PARERR;
        }

        success = sc64_sd_block_read(buff, SD_BLOCK_SIZE);

        if (!success) {
            return RES_ERROR;
        }
    } else {
        success = sc64_sd_cmd_send(CMD18, sector, &response);
        
        if (!success || response) {
            return RES_PARERR;
        }

        for (size_t i = 0; i < count; i++) {
            success = sc64_sd_block_read(buff, SD_BLOCK_SIZE);

            if (!success) {
                return RES_ERROR;
            }

            buff += SD_BLOCK_SIZE;
        }

        success = sc64_sd_cmd_send(CMD12, 0, &response);

        if (!success || response) {
            return RES_ERROR;
        }
    }

    return RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff) {
    return RES_PARERR;
}
