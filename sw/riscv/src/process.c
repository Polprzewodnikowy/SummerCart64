#include "process.h"
#include "usb.h"
#include "cfg.h"
#include "dma.h"
#include "joybus.h"
#include "rtc.h"
#include "i2c.h"
#include "flashram.h"
#include "uart.h"


void process_init (void) {
    usb_init();
    cfg_init();
    dma_init();
    joybus_init();
    rtc_init();
    i2c_init();
    flashram_init();
    uart_init();
}


void process_loop (void) {
    while (1) {
        process_usb();
        process_cfg();
        process_dma();
        process_joybus();
        process_rtc();
        process_i2c();
        process_flashram();
        process_uart();
    }
}
