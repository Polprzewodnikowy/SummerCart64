#include "storage.h"
#include "sc64.h"
#include "fatfs/ff.h"


static const char *fatfs_error_codes[] = {
	"(0) Succeeded",
	"(1) A hard error occurred in the low level disk I/O layer",
	"(2) Assertion failed",
	"(3) The physical drive cannot work",
	"(4) Could not find the file",
	"(5) Could not find the path",
	"(6) The path name format is invalid",
	"(7) Access denied due to prohibited access or directory full",
	"(8) Access denied due to prohibited access",
	"(9) The file/directory object is invalid",
	"(10) The physical drive is write protected",
	"(11) The logical drive number is invalid",
	"(12) The volume has no work area",
	"(13) There is no valid FAT volume",
	"(14) The f_mkfs() aborted due to any problem",
	"(15) Could not get a grant to access the volume within defined period",
	"(16) The operation is rejected according to the file sharing policy",
	"(17) LFN working buffer could not be allocated",
	"(18) Number of open files > FF_FS_LOCK",
	"(19) Given parameter is invalid",
};


#define FF_CHECK(x) { \
    FRESULT fatfs_result = x; \
    if (fatfs_result) { \
        LOG_E("fatfs error \"%s\" at [%s:%d] in expr: %s\r\n", fatfs_error_codes[fatfs_result], __FILE__, __LINE__, #x); \
        while (1); \
    } \
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
        LOG_E("Unknown storage backend %d\r\n", storage_backend);
        while (1);
    }

    FF_CHECK(f_open(&fil, "sc64menu.elf", FA_READ));

    // TODO: Implement ELF loader here

    FF_CHECK(f_close(&fil));

    // TODO: Execute ELF here
    // menu(&boot_info, &sc64_info);

    boot_info->device_type = BOOT_DEVICE_TYPE_ROM;
}
