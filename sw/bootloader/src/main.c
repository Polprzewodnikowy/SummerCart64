#include "boot/boot.h"
#include "loader/loader.h"
#include "sc64/sc64.h"
#include "sc64/sc64_sd_fs.h"


#define DEFAULT_MENU_FILE_PATH "SC64/MENU.z64\0";


static const char *CONFIG_FILE_PATH = "SC64/config.txt";


static menu_load_error_t convert_error(sc64_sd_fs_error_t sd_fs_error) {
    switch (sd_fs_error) {
        case SC64_SD_FS_NO_CARD:
            return E_MENU_ERROR_NO_CARD;
        case SC64_SD_FS_NO_FILESYSTEM:
            return E_MENU_ERROR_NO_FILESYSTEM;
        case SC64_SD_FS_NO_FILE:
            return E_MENU_ERROR_NO_FILE;
        case SC64_SD_FS_READ_ERROR:
            return E_MENU_ERROR_READ_ERROR;
        case SC64_SD_FS_OTHER_ERROR:
            return E_MENU_ERROR_OTHER_ERROR;
        default:
            return E_MENU_OK;
    }
}


int main(void) {
    OS_BOOT_CONFIG->tv_type = TV_NTSC;

    if (sc64_get_version() != SC64_CART_VERSION_A) {
        loader_display_error_and_halt(E_MENU_ERROR_NOT_SC64, "");
    }

    sc64_enable_rom_switch();

    uint32_t boot_mode = sc64_get_boot_mode();

    bool skip_menu = (boot_mode & SC64_CART_BOOT_SKIP_MENU);
    bool cic_seed_override = (boot_mode & SC64_CART_BOOT_CIC_SEED_OVERRIDE);
    bool tv_type_override = (boot_mode & SC64_CART_BOOT_TV_TYPE_OVERRIDE);
    bool ddipl_override = (boot_mode & SC64_CART_BOOT_DDIPL_OVERRIDE);
    bool rom_loaded = (boot_mode & SC64_CART_BOOT_ROM_LOADED);
    tv_type_t tv_type = ((boot_mode & SC64_CART_BOOT_TV_TYPE_MASK) >> SC64_CART_BOOT_TV_TYPE_BIT);
    uint16_t cic_seed = ((boot_mode & SC64_CART_BOOT_CIC_SEED_MASK) >> SC64_CART_BOOT_CIC_SEED_BIT);

    if (!skip_menu) {
        char rom_path[256] = DEFAULT_MENU_FILE_PATH;
        char save_path[256] = "\0";
        sc64_sd_fs_error_t sd_fs_error;
        sc64_sd_fs_config_t config = {
            .rom = rom_path,
            .rom_reload = false,
            .save = save_path,
            .save_type = 0,
            .save_writeback = false,
            .cic_seed = 0xFFFF, 
            .tv_type = -1,
        };

        sd_fs_error = sc64_sd_fs_init();
        if (sd_fs_error != SC64_SD_FS_OK) {
            loader_display_error_and_halt(convert_error(sd_fs_error), "sc64_sd_fs_init");
        }

        sd_fs_error = sc64_sd_fs_load_config(CONFIG_FILE_PATH, &config);
        if ((sd_fs_error != SC64_SD_FS_OK) && (sd_fs_error != SC64_SD_FS_NO_FILE)) {
            loader_display_error_and_halt(convert_error(sd_fs_error), "sc64_sd_fs_load_config");
        }

        if (config.cic_seed != 0xFFFF) {
            cic_seed_override = true;
            cic_seed = config.cic_seed;
        }

        if (config.tv_type != -1) {
            tv_type_override = true;
            tv_type = config.tv_type;
        }

        if (!rom_loaded || config.rom_reload) {
            loader_display_logo();
        }

        if (config.save_type > 0) {
            sc64_disable_eeprom();
            sc64_disable_sram();
            sc64_disable_flashram();

            switch (config.save_type) {
                case 1: sc64_enable_eeprom(false); break;
                case 2: sc64_enable_eeprom(true); break;
                case 3:
                case 4: sc64_enable_sram(); break;
                case 5:
                case 6: sc64_enable_flashram(); break;
            }

            if (config.save_type >= 3 || config.save_type <= 5) {
                sc64_set_save_address(SC64_SDRAM_SIZE - (128 * 1024));
            } else if (config.save_type == 6) {
                sc64_set_save_address(0x01618000);
            }

            if (rom_loaded && (config.save[0] != '\0') && config.save_writeback) {
                sd_fs_error = sc64_sd_fs_store_save(config.save);
                if (sd_fs_error != SC64_SD_FS_OK) {
                    loader_display_error_and_halt(convert_error(sd_fs_error), "sc64_sd_fs_store_save");
                }
            }
        }

        if (!rom_loaded || config.rom_reload) {
            sd_fs_error = sc64_sd_fs_load_rom(config.rom);
            if (sd_fs_error != SC64_SD_FS_OK) {
                loader_display_error_and_halt(convert_error(sd_fs_error), "sc64_sd_fs_load_rom");
            }
            
            sc64_set_boot_mode(boot_mode | SC64_CART_BOOT_ROM_LOADED);
        }

        if ((config.save_type > 0) && (config.save[0] != '\0') && !rom_loaded) {
            sd_fs_error = sc64_sd_fs_load_save(config.save);
            if (sd_fs_error != SC64_SD_FS_OK) {
                loader_display_error_and_halt(convert_error(sd_fs_error), "sc64_sd_fs_load_save");
            }
        }

        sc64_sd_fs_deinit();

        if (!rom_loaded || config.rom_reload) {
            loader_cleanup();
        }
    }

    if (ddipl_override) {
        sc64_enable_ddipl();
    } else {
        sc64_disable_ddipl();
    }

    cart_header_t *cart_header = boot_load_cart_header(ddipl_override);

    if (!cic_seed_override) {
        cic_seed = boot_get_cic_seed(cart_header);
    }

    if (!tv_type_override) {
        tv_type = boot_get_tv_type(cart_header);
    }

    boot(cart_header, cic_seed, tv_type, ddipl_override);
}
