#include "boot.h"
#include "error.h"
#include "init.h"
#include "io.h"
#include "menu.h"
#include "sc64.h"


void main (void) {
    sc64_error_t error;
    boot_info_t boot_info;
    sc64_boot_info_t sc64_boot_info;

    if ((error = sc64_get_boot_info(&sc64_boot_info)) != SC64_OK) {
        error_display("Could not obtain boot info: %d", error);
    }

    switch (sc64_boot_info.boot_mode) {
        case BOOT_MODE_MENU:
            menu_load_and_run();
            break;

        case BOOT_MODE_ROM:
            boot_info.device_type = BOOT_DEVICE_TYPE_ROM;
            break;

        case BOOT_MODE_DDIPL:
            boot_info.device_type = BOOT_DEVICE_TYPE_DD;
            break;

        default:
            error_display("Unknown boot mode selected [%d]\n", sc64_boot_info.boot_mode);
            break;
    }

    boot_info.reset_type = OS_INFO->reset_type;
    boot_info.tv_type = sc64_boot_info.tv_type;
    boot_info.cic_seed = (sc64_boot_info.cic_seed & 0xFF);
    bool detect_cic_seed = (sc64_boot_info.cic_seed == CIC_SEED_AUTO);

    deinit();

    boot(&boot_info, detect_cic_seed);
}
