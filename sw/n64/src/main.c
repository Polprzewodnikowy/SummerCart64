#include "boot.h"
#include "error.h"
#include "sc64.h"
#include "storage.h"


void main (void) {
    boot_info_t boot_info;
    sc64_info_t sc64_info;

    boot_info.reset_type = OS_INFO->reset_type;

    sc64_get_info(&sc64_info);

    switch (sc64_info.boot_mode) {
        case BOOT_MODE_MENU:
            storage_run_menu(STORAGE_BACKEND_SD, &boot_info, &sc64_info);
            break;

        case BOOT_MODE_ROM:
            boot_info.device_type = BOOT_DEVICE_TYPE_ROM;
            break;

        case BOOT_MODE_DDIPL:
            boot_info.device_type = BOOT_DEVICE_TYPE_DD;
            break;

        case BOOT_MODE_DIRECT:
            storage_run_menu(STORAGE_BACKEND_USB, &boot_info, &sc64_info);
            break;

        default:
            error_display("Unknown boot mode selected");
            break;
    }

    if (sc64_info.tv_type != TV_TYPE_UNKNOWN) {
        boot_info.tv_type = sc64_info.tv_type;
    } else {
        if (!boot_get_tv_type(&boot_info)) {
            boot_info.tv_type = OS_INFO->tv_type;
        }
    }

    if (sc64_info.cic_seed != 0xFFFF) {
        boot_info.cic_seed = sc64_info.cic_seed & 0xFF;
        boot_info.version = (sc64_info.cic_seed >> 8) & 0x01;
    } else {
        if (!boot_get_cic_seed_version(&boot_info)) {
            boot_info.cic_seed = 0x3F;
            boot_info.version = 0;
        }
    }

    boot(&boot_info);
}
