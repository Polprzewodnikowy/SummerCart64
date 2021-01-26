#ifndef SC64_SD_H__
#define SC64_SD_H__


#include "platform.h"


#define CMD0                (0x40 + 0)
#define CMD1                (0x40 + 1)
#define CMD6                (0x40 + 6)
#define CMD8                (0x40 + 8)
#define CMD9                (0x40 + 9)
#define CMD10               (0x40 + 10)
#define CMD12               (0x40 + 12)
#define CMD16               (0x40 + 16)
#define CMD17               (0x40 + 17)
#define CMD18               (0x40 + 18)
#define CMD23               (0x40 + 23)
#define CMD24               (0x40 + 24)
#define CMD25               (0x40 + 25)
#define CMD55               (0x40 + 55)
#define CMD58               (0x40 + 58)
#define ACMD13              (0xC0 + 13)
#define ACMD23              (0xC0 + 23)
#define ACMD41              (0xC0 + 41)

#define CARD_TYPE_MMC       (0x01)
#define CARD_TYPE_SD1       (0x02)
#define CARD_TYPE_SD2       (0x04)
#define CARD_TYPE_SDC       (CARD_TYPE_SD1 | CARD_TYPE_SD2)
#define CARD_TYPE_BLOCK     (0x08)

#define BLOCK_SIZE          (512)


uint8_t sc64_sd_init(void);
void sc64_sd_deinit(void);
uint8_t sc64_sd_get_status(void);
uint8_t sc64_sd_get_card_type(void);
uint8_t sc64_sd_cmd_write(uint8_t cmd, uint32_t arg);
uint8_t sc64_sd_block_write(uint8_t *buffer, size_t length, uint8_t token);
uint8_t sc64_sd_block_read(uint8_t *buffer, size_t length);


#endif
