#ifndef BOOT_H__
#define BOOT_H__


#include "platform.h"


struct cart_header_s {
    uint32_t pi_conf;
    uint32_t clock_rate;
    uint32_t boot_addr;
    uint32_t release_addr;
    uint32_t crc_1;
    uint32_t crc_2;
    uint32_t __unused_1[2];
    char name[20];
    uint32_t __unused_2;
    uint32_t format;
    char id[2];
    char country_code;
    uint8_t version;
    uint32_t boot_code[1008];
} __attribute__((packed, aligned(1)));

typedef struct cart_header_s cart_header_t;

struct os_boot_config_s {
    uint32_t tv_type;
    uint32_t rom_type;
    uint32_t rom_base;
    uint32_t reset_type;
    uint32_t cic_id;
    uint32_t version;
    uint32_t mem_size;
    uint8_t app_nmi_buffer[64];
    uint32_t __unused[37];
    uint32_t mem_size_6105;
} __attribute__((packed, aligned(1)));

typedef struct os_boot_config_s os_boot_config_t;


#define OS_BOOT_CONFIG_BASE         (0xA0000300)
#define OS_BOOT_CONFIG              ((os_boot_config_t *) OS_BOOT_CONFIG_BASE)

#define OS_BOOT_ROM_TYPE_GAME_PAK   (0)
#define OS_BOOT_ROM_TYPE_DD         (1)

#define BOOT_SEED_IPL3(x)           (((x) & 0x000000FF) >> 0)
#define BOOT_SEED_OS_VERSION(x)     (((x) & 0x00000100) >> 8)


cart_header_t *boot_load_cart_header(void);
uint16_t boot_get_cic_seed(cart_header_t *cart_header);
tv_type_t boot_get_tv_type(cart_header_t *cart_header);
void boot(cart_header_t *cart_header, uint16_t cic_seed, tv_type_t tv_type, uint32_t ddipl_override);


#endif
