#include "error.h"
#include "fatfs/ff.h"
#include "io.h"
#include "menu.h"
#include "sc64.h"


#define ROM_ADDRESS     (0x10000000)


extern sc64_error_t sc64_error_fatfs;


static const char *fatfs_error_codes[] = {
    "No error",
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
        error_display( \
            message "\n" \
            " > FatFs error: %s\n" \
            " > SD card error: %s (%08X)", \
            __VA_ARGS__ __VA_OPT__(,) \
            fatfs_error_codes[fresult], \
            sc64_error_description(sc64_error_fatfs), sc64_error_fatfs \
        ); \
    } \
}


static void fix_menu_file_size (FIL *fil) {
    fil->obj.objsize = ALIGN(f_size(fil), FF_MAX_SS);
}


void menu_load (void) {
    sc64_error_t error;
    bool writeback_pending;
    FRESULT fresult;
    FATFS fs;
    FIL fil;
    UINT bytes_read;

    do {
        if ((error = sc64_writeback_pending(&writeback_pending)) != SC64_OK) {
            error_display("Command WRITEBACK_PENDING failed\n (%08X) - %s", error, sc64_error_description(error));
        }
    } while (writeback_pending);

    if ((error = sc64_writeback_disable()) != SC64_OK) {
        error_display("Could not disable save writeback\n (%08X) - %s", error, sc64_error_description(error));
    }

    FF_CHECK(f_mount(&fs, "", 1), "SD card initialize error");
    FF_CHECK(f_open(&fil, "sc64menu.n64", FA_READ), "Could not open menu executable (sc64menu.n64)");
    fix_menu_file_size(&fil);
    FF_CHECK(f_read(&fil, (void *) (ROM_ADDRESS), f_size(&fil), &bytes_read), "Could not read menu file");
    FF_CHECK((bytes_read != f_size(&fil)) ? FR_INT_ERR : FR_OK, "Read size is different than expected");
    FF_CHECK(f_close(&fil), "Could not close menu file");
    FF_CHECK(f_unmount(""), "Could not unmount drive");
}
