#include "error.h"
#include "fatfs/ff.h"
#include "init.h"
#include "io.h"
#include "menu.h"
#include "sc64.h"


extern const void __bootloader_start __attribute__((section(".data")));
extern const void __bootloader_end __attribute__((section(".data")));


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
        error_display(message ".\nReason: %s", __VA_ARGS__ __VA_OPT__(,) fatfs_error_codes[fresult]); \
    } \
}


static void menu_check_load_address (void *address, size_t size) {
    void *menu_start = address;
    void *menu_end = (address + size);
    void *usable_ram_start = (void *) (0x80000400UL);
    void *usable_ram_end = (void *) (0x80400000UL);
    void *bootloader_start = (void *) (&__bootloader_start);
    void *bootloader_end = (void *) (&__bootloader_end);

    if ((menu_start < usable_ram_start) || (menu_end > usable_ram_end)) {
        error_display("Incorrect menu load address/size.\nReason: Outside of usable RAM space\n");
    }

    if ((menu_start < bootloader_end) && (bootloader_start < menu_end)) {
        error_display("Incorrect menu load address/size.\nReason: Overlapping bootloader space\n");
    }
}


void menu_load_and_run (void) {
    sc64_error_t error;
    bool writeback_pending;

    do {
        if ((error = sc64_writeback_pending(&writeback_pending)) != SC64_OK) {
            error_display("Command WRITEBACK_PENDING failed: %d", error);
        }
    } while (writeback_pending);

    if ((error = sc64_writeback_disable()) != SC64_OK) {
        error_display("Could not disable writeback: %d", error);
    }

    void (* menu)(void);
    FRESULT fresult;
    FATFS fs;
    FIL fil;
    UINT br;
    size_t size = ROM_MAX_LOAD_SIZE;

    FF_CHECK(f_mount(&fs, "", 1), "SD card initialize error. No SD card or invalid partition table");
    FF_CHECK(f_open(&fil, "sc64menu.n64", FA_READ), "Could not open menu executable (sc64menu.n64)");
    FF_CHECK(f_lseek(&fil, ROM_ENTRY_OFFSET), "Could not seek to entry point offset");
    FF_CHECK(f_read(&fil, &menu, sizeof(menu), &br), "Could not read entry point");
    FF_CHECK(f_lseek(&fil, ROM_CODE_OFFSET), "Could not seek to code start offset");
    if ((f_size(&fil) - ROM_CODE_OFFSET) < size) {
        size = (size_t) (f_size(&fil) - ROM_CODE_OFFSET);
    }
    menu_check_load_address(menu, size);
    cache_data_hit_writeback_invalidate(menu, size);
    cache_inst_hit_invalidate(menu, size);
    FF_CHECK(f_read(&fil, menu, size, &br), "Could not read menu file");
    FF_CHECK((br != size) ? FR_INT_ERR : FR_OK, "Read size is different than expected");
    FF_CHECK(f_close(&fil), "Could not close menu file");
    FF_CHECK(f_unmount(""), "Could not unmount drive");

    deinit();

    menu();

    while (1);
}
