#include "button.h"
#include "cfg.h"
#include "dd.h"
#include "flashram.h"
#include "fpga.h"
#include "isv.h"
#include "rtc.h"
#include "sd.h"
#include "usb.h"
#include "writeback.h"


void gvr_task (void) {
    while (fpga_id_get() != FPGA_ID);

    button_init();
    cfg_init();
    dd_init();
    flashram_init();
    isv_init();
    sd_init();
    usb_init();
    writeback_init();

    while (1) {
        button_process();
        cfg_process();
        dd_process();
        flashram_process();
        isv_process();
        rtc_process();
        sd_process();
        usb_process();
        writeback_process();
    }
}
