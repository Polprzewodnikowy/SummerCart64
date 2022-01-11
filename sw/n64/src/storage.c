#include <assert.h>
#include "error.h"
#include "sc64.h"
#include "storage.h"
#include "fatfs/ff.h"


#define FF_CHECK(x) { \
    FRESULT fatfs_result = x; \
    assert(fatfs_result == FR_OK); \
}


void storage_run_menu (storage_backend_t storage_backend, boot_info_t *boot_info, sc64_info_t *sc64_info) {
    FATFS fs;
    FIL fil;

    if (storage_backend == STORAGE_BACKEND_SD) {
        FF_CHECK(f_mount(&fs, "0:", 1));
        FF_CHECK(f_chdrive("0:"));
    } else if (storage_backend == STORAGE_BACKEND_USB) {
        FF_CHECK(f_mount(&fs, "1:", 1));
        FF_CHECK(f_chdrive("1:"));
    } else {
        error_display("Unknown storage backend");
    }

    FF_CHECK(f_open(&fil, "sc64menu.elf", FA_READ));

    // TODO: Implement ELF loader here

    FF_CHECK(f_close(&fil));

    // TODO: Execute ELF here
    // menu(&boot_info, &sc64_info);

    boot_info->device_type = BOOT_DEVICE_TYPE_ROM;
}
