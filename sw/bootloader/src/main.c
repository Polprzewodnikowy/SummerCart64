#include "boot.h"
#include "cic.h"
#include "error.h"
#include "init.h"
#include "io.h"
#include "joybus.h"
#include "menu.h"
#include "sc64.h"


void main (void) {
    sc64_error_t error;
    sc64_boot_params_t sc64_boot_params;

    if ((error = sc64_get_boot_params(&sc64_boot_params)) != SC64_OK) {
        error_display("Could not obtain boot info\n (%08X) - %s", error, sc64_error_description(error));
    }

    boot_params_t boot_params;

    boot_params.reset_type = BOOT_RESET_TYPE_COLD;
    boot_params.tv_type = sc64_boot_params.tv_type;
    boot_params.cic_seed = (sc64_boot_params.cic_seed & 0xFF);
    boot_params.detect_cic_seed = (sc64_boot_params.cic_seed == CIC_SEED_AUTO);

    switch (sc64_boot_params.boot_mode) {
        case BOOT_MODE_ROM:
        case BOOT_MODE_DDIPL: {
            joybus_controller_state_t controller_state;
            if (joybus_get_controller_state(0, &controller_state) && controller_state.r) {
                sc64_boot_params.boot_mode = BOOT_MODE_MENU;
            }
            break;
        }

        default:
            break;
    }

    switch (sc64_boot_params.boot_mode) {
        case BOOT_MODE_MENU:
            menu_load();
            boot_params.device_type = BOOT_DEVICE_TYPE_ROM;
            boot_params.reset_type = BOOT_RESET_TYPE_NMI;
            boot_params.cic_seed = cic_get_seed(CIC_6102_7101);
            boot_params.detect_cic_seed = false;
            break;

        case BOOT_MODE_ROM:
            boot_params.device_type = BOOT_DEVICE_TYPE_ROM;
            break;

        case BOOT_MODE_DDIPL:
            boot_params.device_type = BOOT_DEVICE_TYPE_64DD;
            break;

        default:
            error_display("Unknown boot mode selected (%d)\n", sc64_boot_params.boot_mode);
            break;
    }

    deinit();

    boot(&boot_params);
}
