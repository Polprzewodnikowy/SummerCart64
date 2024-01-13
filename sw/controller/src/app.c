#include "button.h"
#include "cfg.h"
#include "cic.h"
#include "dd.h"
#include "flashram.h"
#include "fpga.h"
#include "hw.h"
#include "isv.h"
#include "rtc.h"
#include "sd.h"
#include "timer.h"
#include "usb.h"
#include "writeback.h"


void app (void) {
    hw_app_init();

    timer_init();

    while (fpga_id_get() != FPGA_ID);

    rtc_init();

    button_init();
    cfg_init();
    cic_init();
    dd_init();
    flashram_init();
    isv_init();
    sd_init();
    usb_init();
    writeback_init();

    while (1) {
        button_process();
        cfg_process();
        cic_process();
        dd_process();
        flashram_process();
        isv_process();
        rtc_process();
        sd_process();
        usb_process();
        writeback_process();
    }
}
