#include "error.h"
#include "init.h"
#include "storage.h"
#include "fatfs/ff.h"


#define ROM_ENTRY_OFFSET    (8)
#define ROM_CODE_OFFSET     (4096)
#define ROM_MAX_LOAD_SIZE   (1 * 1024 * 1024)


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


#define FF_CHECK(x, message, ...) { \
    fresult = x; \
    if (fresult != FR_OK) { \
        error_display(message " [%s]:\n %s\n", __VA_ARGS__ __VA_OPT__(,) #x, fatfs_error_codes[fresult]); \
    } \
}


void storage_run_menu (storage_backend_t storage_backend) {
    void (* menu)(void);
    FRESULT fresult;
    FATFS fs;
    FIL fil;
    UINT br;
    FSIZE_t size = ROM_MAX_LOAD_SIZE;
    const TCHAR *path = "";

    if (storage_backend == STORAGE_BACKEND_SD) {
        path = "0:";
    } else if (storage_backend == STORAGE_BACKEND_USB) {
        path = "1:";
    } else {
        error_display("Unknown storage backend [%d]\n", storage_backend);
    }

    FF_CHECK(f_mount(&fs, path, 1), "Couldn't mount drive");
    FF_CHECK(f_chdrive(path), "Couldn't chdrive");
    FF_CHECK(f_open(&fil, "sc64menu.n64", FA_READ), "Couldn't open menu file");
    FF_CHECK(f_lseek(&fil, 0), "debug 1");
    FF_CHECK(f_read(&fil, (void *) (0x10000000UL), f_size(&fil), &br), "debug 2");
    FF_CHECK(f_lseek(&fil, ROM_ENTRY_OFFSET), "Couldn't seek to entry point offset");
    FF_CHECK(f_read(&fil, &menu, sizeof(menu), &br), "Couldn't read entry point");
    FF_CHECK(f_lseek(&fil, ROM_CODE_OFFSET), "Couldn't seek to code start offset");
    if ((f_size(&fil) - ROM_CODE_OFFSET) < size) {
        size = (f_size(&fil) - ROM_CODE_OFFSET);
    }
    FF_CHECK(f_read(&fil, menu, size, &br), "Couldn't read menu file");
    FF_CHECK(br != size, "Read size is different than expected");
    FF_CHECK(f_close(&fil), "Couldn't close menu file");
    FF_CHECK(f_unmount(path), "Couldn't unmount drive");

    deinit();

    menu();

    while (1);
}
