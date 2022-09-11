#include "ff.h"
#include "diskio.h"
#include "../io.h"
#include "../sc64.h"
#include "../error.h"


#define FROM_BCD(x)     ((((x >> 4) & 0x0F) * 10) + (x & 0x0F))


static DSTATUS status = STA_NOINIT;


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
    if (!sc64_sd_card_init()) {
        status &= ~(STA_NOINIT);
    }
    return status;
}

DRESULT disk_read (BYTE pdrv, BYTE *buff, LBA_t sector, UINT count) {
    if (pdrv > 0) {
        return RES_PARERR;
    }
    uint32_t *physical_address = (uint32_t *) (PHYSICAL(buff));
    if (physical_address < (uint32_t *) (N64_RAM_SIZE)) {
        while (count > 0) {
            uint32_t block = ((count > 16) ? 16 : count);
            if (sc64_sd_read_sectors((uint32_t *) (SC64_BUFFERS->BUFFER), sector, block)) {
                return RES_ERROR;
            }
            for (uint32_t i = 0; i < (block * 512); i += 4) {
                // TODO: use dma
                uint32_t data = pi_io_read((uint32_t *) (&SC64_BUFFERS->BUFFER[i]));
                uint8_t *ptr = (uint8_t *) (&data);
                for (int j = 0; j < 4; j++) {
                    *buff++ = *ptr++;
                }
            }
            count -= block;
            sector += block;
        }
    } else {
        if (sc64_sd_read_sectors(physical_address, sector, count)) {
            return RES_ERROR;
        }
    }
    return RES_OK;
}

#if !FF_FS_READONLY
DRESULT disk_write (BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count) {
    if (pdrv > 0) {
        return RES_PARERR;
    }
    // TODO: write
    return RES_ERROR;
}
#endif

DRESULT disk_ioctl (BYTE pdrv, BYTE cmd, void *buff) {
    if (pdrv > 0) {
        return RES_PARERR;
    }
    if (cmd == CTRL_SYNC) {
        return RES_OK;
    }
    return RES_PARERR;
}

DWORD get_fattime(void) {
    rtc_time_t t;
    sc64_get_time(&t);
    return (
        ((FROM_BCD(t.year) + 20) << 25) |
        (FROM_BCD(t.month) << 21) |
        (FROM_BCD(t.day) << 16) |
        (FROM_BCD(t.hour) << 11) |
        (FROM_BCD(t.minute) << 5) |
        (FROM_BCD(t.second) >> 1)
    );
}
