#include "error.h"
#include "init.h"
#include "interrupts.h"
#include "io.h"
#include "sc64.h"
#include "test.h"


init_tv_type_t __tv_type;
init_reset_type_t __reset_type;
uint32_t __entropy;


void init (init_tv_type_t tv_type, init_reset_type_t reset_type, uint32_t entropy) {
    sc64_error_t error;

    __tv_type = tv_type;
    __reset_type = reset_type;
    __entropy = entropy;

    sc64_unlock();

    if (!sc64_check_presence()) {
        error_display("SC64 hardware not detected");
    }

    interrupts_init();
    interrupts_start_watchdog();

    if ((error = sc64_set_config(CFG_ID_BOOTLOADER_SWITCH, false)) != SC64_OK) {
        error_display("Command CONFIG_SET [BOOTLOADER_SWITCH] failed\n (%08X) - %s", error, sc64_error_description(error));
    }

    if (test_check()) {
        interrupts_stop_watchdog();
        test_execute();
    }
}

void deinit (void) {
    interrupts_stop_watchdog();
    interrupts_disable();

    sc64_lock();
}
