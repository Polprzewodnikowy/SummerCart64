#include "ff.h"
#include "diskio.h"

#include "sc64_sd.h"


static LBA_t current_sector = 0;
static uint8_t in_transaction = FALSE;


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
            current_sector = 0;
            in_transaction = FALSE;

            return 0;
        }
    } else {
        return 0;
    }

    return STA_NOINIT;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count) {
    uint8_t card_type = sc64_sd_get_card_type();
    uint8_t cmd_response = 0;
    uint8_t data_result = FALSE;

    if ((pdrv > 0) || (count == 0)) {
        return RES_PARERR;
    }

    if (!sc64_sd_get_status()) {
        return RES_NOTRDY;
    }

    if (!(card_type & CARD_TYPE_BLOCK)) {
        sector *= BLOCK_SIZE;
    }

    if (in_transaction && (sector != current_sector)) {
        cmd_response = sc64_sd_cmd_write(CMD12, 0);
        in_transaction = FALSE;
    }

    if (cmd_response == 0) {
        if (!in_transaction) {
            if(sc64_sd_cmd_write(CMD18, sector) == 0) {
                in_transaction = TRUE;
                current_sector = sector;
            }
        }

        for (size_t i = 0; i < count; i++) {
            data_result = sc64_sd_block_read(buff, BLOCK_SIZE);

            if (!data_result) {
                in_transaction = FALSE;

                break;
            }

            buff += BLOCK_SIZE;
            current_sector += (card_type & CARD_TYPE_BLOCK) ? BLOCK_SIZE : 1;
        }
    }

    return data_result ? RES_OK : RES_ERROR;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff) {
    return RES_PARERR;
}
