#include <assert.h>
#include "error.h"
#include "sc64.h"
#include "storage.h"
#include "fatfs/ff.h"


static const char *fatfs_error_codes[] = {
	"Succeeded [0]",
	"A hard error occurred in the low level disk I/O layer [1]",
	"Assertion failed [2]",
	"The physical drive cannot work [3]",
	"Could not find the file [4]",
	"Could not find the path [5]",
	"The path name format is invalid [6]",
	"Access denied due to prohibited access or directory full [7]",
	"Access denied due to prohibited access [8]",
	"The file/directory object is invalid [9]",
	"The physical drive is write protected [10]",
	"The logical drive number is invalid [11]",
	"The volume has no work area [12]",
	"There is no valid FAT volume [13]",
	"The f_mkfs() aborted due to any problem [14]",
	"Could not get a grant to access the volume within defined period [15]",
	"The operation is rejected according to the file sharing policy [16]",
	"LFN working buffer could not be allocated [17]",
	"Number of open files > FF_FS_LOCK [18]",
	"Given parameter is invalid [19]",
};


#define FF_CHECK(x, message) { \
    FRESULT fatfs_result = x; \
    if (fatfs_result != FR_OK) { \
        error_display("%s:\n %s", message, fatfs_error_codes[fatfs_result]); \
    } \
}


void storage_run_menu (storage_backend_t storage_backend, boot_info_t *boot_info, sc64_info_t *sc64_info) {
    FATFS fs;
    FIL fil;

    if (storage_backend == STORAGE_BACKEND_SD) {
        FF_CHECK(f_mount(&fs, "0:", 1), "Couldn't mount SD drive");
        FF_CHECK(f_chdrive("0:"), "Couldn't chdrive to SD drive");
    } else if (storage_backend == STORAGE_BACKEND_USB) {
        FF_CHECK(f_mount(&fs, "1:", 1), "Couldn't mount USB drive");
        FF_CHECK(f_chdrive("1:"), "Couldn't chdrive to USB drive");
    } else {
        error_display("Unknown storage backend [%d]", storage_backend);
    }

    FF_CHECK(f_open(&fil, "sc64menu.elf", FA_READ), "Couldn't open menu file");

    // TODO: Implement ELF loader here

    FF_CHECK(f_close(&fil), "Couldn't close menu file");

    // TODO: Execute ELF here
    // menu(&boot_info, &sc64_info);

    boot_info->device_type = BOOT_DEVICE_TYPE_ROM;
}
