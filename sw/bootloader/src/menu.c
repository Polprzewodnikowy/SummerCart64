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
    "The physical drive does not work",
    "Could not find the file",
    "Could not find the path",
    "The path name format is invalid",
    "Access denied due to a prohibited access or directory full",
    "Access denied due to a prohibited access",
    "The file/directory object is invalid",
    "The physical drive is write protected",
    "The logical drive number is invalid",
    "The volume has no work area",
    "Could not find a valid FAT volume",
    "The f_mkfs function aborted due to some problem",
    "Could not take control of the volume within defined period",
    "The operation is rejected according to the file sharing policy",
    "LFN working buffer could not be allocated or given buffer is insufficient in size",
    "Number of open files > FF_FS_LOCK",
    "Given parameter is invalid",
};


static void menu_fix_file_size (FIL *fil) {
    fil->obj.objsize = ALIGN(f_size(fil), FF_MAX_SS);
}

static void menu_error_display (const char *message, FRESULT fresult) {
    error_display(
        " > FatFs error: %s\n"
        " > SD card error: %s (%08X)\n"
        "\n"
        "[ %s ]\n"
        " > Please insert a FAT32/exFAT formatted SD card.\n"
        " > To start the menu please put \"sc64menu.n64\" file\n"
        "   in the top directory on the SD card.\n"
        " > Latest menu version is available on the\n"
        "   https://menu.summercart64.dev website.\n",
        fatfs_error_codes[fresult],
        sc64_error_description(sc64_error_fatfs),
        sc64_error_fatfs,
        message
    );
}


#define FF_CHECK(x, message) { \
    fresult = x; \
    if (fresult != FR_OK) { \
        menu_error_display(message, fresult); \
    } \
}


void menu_load (void) {
    sc64_error_t error;
    bool writeback_pending;
    sc64_sd_card_status_t sd_card_status;
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

    if ((error = sc64_sd_card_get_status(&sd_card_status)) != SC64_OK) {
        error_display("Could not get SD card status\n (%08X) - %s", error, sc64_error_description(error));
    }

    if (!(sd_card_status & SD_CARD_STATUS_INSERTED)) {
        menu_error_display("No SD card detected in the slot", FR_OK);
    }

    FF_CHECK(f_mount(&fs, "", 1), "SD card initialize error");
    FF_CHECK(f_open(&fil, "sc64menu.n64", FA_READ), "Could not open menu executable");
    menu_fix_file_size(&fil);
    FF_CHECK(f_read(&fil, (void *) (ROM_ADDRESS), f_size(&fil), &bytes_read), "Could not read menu file");
    FF_CHECK((bytes_read != f_size(&fil)) ? FR_INT_ERR : FR_OK, "Read size is different than expected");
    FF_CHECK(f_close(&fil), "Could not close menu file");
    FF_CHECK(f_unmount(""), "Could not unmount drive");
}
