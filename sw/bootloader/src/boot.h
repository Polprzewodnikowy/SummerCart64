#ifndef BOOT_H__
#define BOOT_H__


#include <stdbool.h>
#include <stdint.h>


typedef enum {
    BOOT_DEVICE_TYPE_ROM = 0,
    BOOT_DEVICE_TYPE_64DD = 1,
} boot_device_type_t;

typedef enum {
    BOOT_RESET_TYPE_COLD = 0,
    BOOT_RESET_TYPE_NMI = 1,
} boot_reset_type_t;

typedef enum {
    BOOT_TV_TYPE_PAL = 0,
    BOOT_TV_TYPE_NTSC = 1,
    BOOT_TV_TYPE_MPAL = 2,
    BOOT_TV_TYPE_PASSTHROUGH = 3,
} boot_tv_type_t;


typedef struct {
    boot_device_type_t device_type;
    boot_reset_type_t reset_type;
    boot_tv_type_t tv_type;
    uint8_t cic_seed;
    bool detect_cic_seed;
} boot_params_t;


void boot (boot_params_t *params);


#endif
