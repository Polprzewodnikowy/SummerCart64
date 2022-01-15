#include "error.h"
#include "init.h"
#include "storage.h"
#include "fatfs/ff.h"


static const char *fatfs_error_codes[] = {
	"Succeeded",
	"A hard error occurred in the low level disk I/O layer",
	"Assertion failed",
	"The physical drive cannot work",
	"Could not find the file",
	"Could not find the path",
	"The path name format is invalid",
	"Access denied due to prohibited access or directory full",
	"Access denied due to prohibited access",
	"The file/directory object is invalid",
	"The physical drive is write protected",
	"The logical drive number is invalid",
	"The volume has no work area",
	"There is no valid FAT volume",
	"The f_mkfs() aborted due to any problem",
	"Could not get a grant to access the volume within defined period",
	"The operation is rejected according to the file sharing policy",
	"LFN working buffer could not be allocated",
	"Number of open files > FF_FS_LOCK",
	"Given parameter is invalid",
};


#define FF_CHECK(x, message) { \
    FRESULT fatfs_result = x; \
    if (fatfs_result != FR_OK) { \
        error_display("%s:\n %s\n", message, fatfs_error_codes[fatfs_result]); \
    } \
}


void storage_run_menu (storage_backend_t storage_backend) {
    FATFS fs;
    FIL fil;

    if (storage_backend == STORAGE_BACKEND_SD) {
        FF_CHECK(f_mount(&fs, "0:", 1), "Couldn't mount SD drive");
        FF_CHECK(f_chdrive("0:"), "Couldn't chdrive to SD drive");
    } else if (storage_backend == STORAGE_BACKEND_USB) {
        FF_CHECK(f_mount(&fs, "1:", 1), "Couldn't mount USB drive");
        FF_CHECK(f_chdrive("1:"), "Couldn't chdrive to USB drive");
    } else {
        error_display("Unknown storage backend [%d]\n", storage_backend);
    }

    FF_CHECK(f_open(&fil, "sc64menu.elf", FA_READ), "Couldn't open menu file");

    // TODO: Implement ELF loader here

    FF_CHECK(f_close(&fil), "Couldn't close menu file");

    deinit();

    // menu();
}
