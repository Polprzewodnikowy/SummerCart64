#include "dd.h"
#include "cfg.h"
#include "flashram.h"
#include "fpga.h"
#include "isv.h"
#include "rtc.h"
#include "usb.h"


void gvr_task (void) {
    while (fpga_id_get() != FPGA_ID);

    cfg_init();
    dd_init();
    flashram_init();
    isv_init();
    usb_init();

    while (1) {
        cfg_process();
        dd_process();
        flashram_process();
        isv_process();
        rtc_process();
        usb_process();
    }
}
