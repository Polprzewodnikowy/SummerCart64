#ifndef BOOT_H__
#define BOOT_H__

#define BOOT_CRC32_5101             (0x587BD543)
#define BOOT_CRC32_6101             (0x6170A4A1)
#define BOOT_CRC32_7102             (0x009E9EA3)
#define BOOT_CRC32_X102             (0x90BB6CB5)
#define BOOT_CRC32_X103             (0x0B050EE0)
#define BOOT_CRC32_X105             (0x98BC2C86)
#define BOOT_CRC32_X106             (0xACC8580A)
#define BOOT_CRC32_8303             (0x0E018159)

#define BOOT_SEED_5101              (0x0000AC00)
#define BOOT_SEED_X101              (0x00043F3F)
#define BOOT_SEED_X102              (0x00003F3F)
#define BOOT_SEED_X103              (0x0000783F)
#define BOOT_SEED_X105              (0x0000913F)
#define BOOT_SEED_X106              (0x0000853F)
#define BOOT_SEED_8303              (0x0000DD00)

#define BOOT_SEED_IPL3(x)           (((x) & 0x0000FF00) >> 8)
#define BOOT_SEED_OS_VERSION(x)     (((x) & 0x00040000) >> 18)

typedef enum cic_type_e {
    E_CIC_TYPE_UNKNOWN,
    E_CIC_TYPE_5101,
    E_CIC_TYPE_X101,
    E_CIC_TYPE_X102,
    E_CIC_TYPE_X103,
    E_CIC_TYPE_X105,
    E_CIC_TYPE_X106,
    E_CIC_TYPE_8303,
    E_CIC_TYPE_END,
} cic_type_t;

typedef enum tv_type_e {
    E_TV_TYPE_PAL,
    E_TV_TYPE_NTSC,
    E_TV_TYPE_MPAL,
    E_TV_TYPE_UNKNOWN,
} tv_type_t;

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

cart_header_t *boot_load_cart_header(void);
cic_type_t boot_get_cic_type(cart_header_t *cart_header);
tv_type_t boot_get_tv_type(cart_header_t *cart_header);
void boot(cart_header_t *cart_header, cic_type_t cic_type, tv_type_t tv_type);

#endif
