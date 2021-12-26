#include "process.h"
#include "usb.h"
#include "cfg.h"
#include "dma.h"
#include "joybus.h"
#include "rtc.h"
#include "i2c.h"
#include "flashram.h"
#include "dd.h"
#include "uart.h"
#include "isv.h"


static const void (*process_table[])(void) = {
    process_usb,
    process_cfg,
    process_rtc,
    process_i2c,
    process_flashram,
    process_dd,
    process_uart,
    process_isv,
    NULL,
};


__attribute__((naked)) void process_loop (void) {
    void (**process_func)(void) = process_table;

    usb_init();
    cfg_init();
    dma_init();
    joybus_init();
    rtc_init();
    i2c_init();
    flashram_init();
    dd_init();
    uart_init();
    isv_init();

    while (1) {
        process_joybus();
        (*process_func++)();
        if (*process_func == NULL) {
            process_func = process_table;
        }
    }
}
