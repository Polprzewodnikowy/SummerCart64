#ifndef BOOT_H__
#define BOOT_H__


#include <stdbool.h>
#include <stdint.h>


typedef enum {
    BOOT_DEVICE_TYPE_ROM = 0,
    BOOT_DEVICE_TYPE_DD = 1,
} boot_device_type_t;

typedef enum {
    BOOT_RESET_TYPE_COLD = 0,
    BOOT_RESET_TYPE_NMI = 1,
} boot_reset_type_t;

typedef enum {
    BOOT_TV_TYPE_PAL = 0,
    BOOT_TV_TYPE_NTSC = 1,
    BOOT_TV_TYPE_MPAL = 2,
} boot_tv_type_t;


typedef struct {
    boot_device_type_t device_type;
    boot_reset_type_t reset_type;
    boot_tv_type_t tv_type;
    uint8_t cic_seed;
    uint8_t version;
} boot_info_t;


typedef struct {
    uint32_t tv_type;
    uint32_t device_type;
    uint32_t device_base;
    uint32_t reset_type;
    uint32_t cic_id;
    uint32_t version;
    uint32_t mem_size;
    uint8_t app_nmi_buffer[64];
    uint32_t __reserved_1[37];
    uint32_t mem_size_6105;
} os_info_t;

#define OS_INFO_BASE                (0x80000300UL)
#define OS_INFO                     ((os_info_t *) OS_INFO_BASE)


bool boot_get_tv_type (boot_info_t *info);
bool boot_get_cic_seed_version (boot_info_t *info);
void boot (boot_info_t *info);


#endif
