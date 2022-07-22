#include "dd.h"
#include "cfg.h"
#include "flashram.h"
#include "fpga.h"
#include "rtc.h"
#include "usb.h"


void gvr_task (void) {
    while (fpga_id_get() != FPGA_ID);

    dd_init();
    cfg_init();
    flashram_init();
    usb_init();

    while (1) {
        dd_process();
        cfg_process();
        flashram_process();
        usb_process();
        rtc_process();
    }
}
