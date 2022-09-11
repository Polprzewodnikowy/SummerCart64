#include <string.h>
#include "ff.h"
#include "diskio.h"
#include "../io.h"
#include "../sc64.h"
#include "../error.h"


#define SD_BLOCK_SIZE       (512)
#define BUFFER_BLOCKS_MAX   (sizeof(SC64_BUFFERS->BUFFER) / SD_BLOCK_SIZE)
#define FROM_BCD(x)         ((((x >> 4) & 0x0F) * 10) + (x & 0x0F))


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
        uint8_t aligned_buffer[BUFFER_BLOCKS_MAX * SD_BLOCK_SIZE] __attribute__((aligned(8)));
        while (count > 0) {
            uint32_t blocks = ((count > BUFFER_BLOCKS_MAX) ? BUFFER_BLOCKS_MAX : count);
            size_t length = (blocks * SD_BLOCK_SIZE);
            if (sc64_sd_read_sectors((uint32_t *) (SC64_BUFFERS->BUFFER), sector, blocks)) {
                return RES_ERROR;
            }
            if (((uint32_t) (buff) % 8) == 0) {
                pi_dma_read((io32_t *) (SC64_BUFFERS->BUFFER), buff, length);
                cache_data_hit_invalidate(buff, length);
            } else {
                pi_dma_read((io32_t *) (SC64_BUFFERS->BUFFER), aligned_buffer, length);
                cache_data_hit_invalidate(aligned_buffer, length);
                memcpy(buff, aligned_buffer, length);
            }
            buff += length;
            sector += blocks;
            count -= blocks;
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
