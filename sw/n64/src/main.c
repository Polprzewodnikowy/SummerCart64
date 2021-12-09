#include "boot.h"
#include "sc64.h"
#include "storage.h"


void main (void) {
    boot_info_t boot_info;
    sc64_info_t sc64_info;

    sc64_get_info(&sc64_info);

    LOG_I("Bootloader version: %.32s\r\n", sc64_info.bootloader_version);

    switch (sc64_info.boot_mode) {
        case BOOT_MODE_MENU:
            LOG_I("Running menu from SD card\r\n");
            storage_run_menu(STORAGE_BACKEND_SD, &boot_info, &sc64_info);
            break;

        case BOOT_MODE_ROM:
            boot_info.device_type = BOOT_DEVICE_TYPE_ROM;
            LOG_I("Running ROM from SDRAM\r\n");
            break;

        case BOOT_MODE_DDIPL:
            boot_info.device_type = BOOT_DEVICE_TYPE_DD;
            LOG_I("Running DDIPL from SDRAM\r\n");
            break;

        case BOOT_MODE_DIRECT:
            LOG_I("Running bootloader from SDRAM, running menu from FSD\r\n");
            storage_run_menu(STORAGE_BACKEND_USB, &boot_info, &sc64_info);
            break;

        default:
            LOG_E("Unknown boot mode! - %d\r\n", sc64_info.boot_mode);
            while (1);
    }

    boot_info.reset_type = OS_INFO->reset_type;

    if (sc64_info.tv_type != TV_TYPE_UNKNOWN) {
        boot_info.tv_type = sc64_info.tv_type;
        LOG_I("Using provided TV type: %d\r\n", boot_info.tv_type);
    } else {
        if (boot_get_tv_type(&boot_info)) {
            LOG_I("Using TV type guessed from ROM header: %d\r\n", boot_info.tv_type);
        } else {
            boot_info.tv_type = OS_INFO->tv_type;
            LOG_I("Using console TV type: %d\r\n", boot_info.tv_type);
        }
    }

    if (sc64_info.cic_seed != 0xFFFF) {
        boot_info.cic_seed = sc64_info.cic_seed & 0xFF;
        boot_info.version = (sc64_info.cic_seed >> 8) & 0x01;
        LOG_I("Using provided CIC seed and version: 0x%02X / %d\r\n", boot_info.cic_seed, boot_info.version);
    } else {
        if (boot_get_cic_seed_version(&boot_info)) {
            LOG_I("Using CIC seed and version guessed from IPL3: 0x%02X / %d\r\n", boot_info.cic_seed, boot_info.version);
        } else {
            boot_info.cic_seed = 0x3F;
            boot_info.version = 0;
            LOG_I("Using 6102/7101 CIC seed and version: 0x%02X / %d\r\n", boot_info.cic_seed, boot_info.version);
        }
    }

    LOG_I("Booting IPL3\r\n\r\n");

    boot(&boot_info);
}
