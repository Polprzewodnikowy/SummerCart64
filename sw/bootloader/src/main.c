#include "boot/boot.h"
#include "loader/loader.h"
#include "sc64/sc64.h"
#include "sc64/sc64_sd_fs.h"


static const char *MENU_FILE_PATH = "SC64/MENU.z64";


int main(void) {
    loader_init();

    if (sc64_get_version() != SC64_CART_VERSION_A) {
        loader_display_error_and_halt(E_MENU_ERROR_NOT_SC64, "");
    }

    sc64_enable_rom_switch();

    uint32_t boot_mode = sc64_get_boot_mode();

    uint32_t skip_menu = (boot_mode & SC64_CART_BOOT_SKIP_MENU);
    uint32_t cic_seed_override = (boot_mode & SC64_CART_BOOT_CIC_SEED_OVERRIDE);
    uint32_t tv_type_override = (boot_mode & SC64_CART_BOOT_TV_TYPE_OVERRIDE);
    uint32_t ddipl_override = (boot_mode & SC64_CART_BOOT_DDIPL_OVERRIDE);
    tv_type_t tv_type = ((boot_mode & SC64_CART_BOOT_TV_TYPE_MASK) >> SC64_CART_BOOT_TV_TYPE_BIT);
    uint16_t cic_seed = ((boot_mode & SC64_CART_BOOT_CIC_SEED_MASK) >> SC64_CART_BOOT_CIC_SEED_BIT);

    if (!skip_menu) {
        sc64_sd_fs_error_t sd_error = sc64_sd_fs_load_rom(MENU_FILE_PATH);

        menu_load_error_t error = E_MENU_OK;

        switch (sd_error) {
            case SC64_SD_FS_NO_CARD:
                error = E_MENU_ERROR_NO_CARD;
                break;
            case SC64_SD_FS_NO_FILESYSTEM:
                error = E_MENU_ERROR_NO_FILESYSTEM;
                break;
            case SC64_SD_FS_NO_FILE:
                error = E_MENU_ERROR_NO_FILE;
                break;
            case SC64_SD_FS_READ_ERROR:
                error = E_MENU_ERROR_READ_ERROR;
                break;
            case SC64_SD_FS_OTHER_ERROR:
                error = E_MENU_ERROR_OTHER_ERROR;
                break;
            default:
                break;
        }

        if (error != E_MENU_OK) {
            loader_display_error_and_halt(error, MENU_FILE_PATH);
        }
    }

    if (ddipl_override) {
        sc64_enable_ddipl();
    } else {
        sc64_disable_ddipl();
    }

    cart_header_t *cart_header = boot_load_cart_header();

    if (!cic_seed_override) {
        cic_seed = boot_get_cic_seed(cart_header);
    }

    if (!tv_type_override) {
        tv_type = boot_get_tv_type(cart_header);
    }

    loader_cleanup();

    boot(cart_header, cic_seed, tv_type, ddipl_override);
}
