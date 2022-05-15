#include "cfg.h"
#include "flashram.h"
#include "fpga.h"
#include "rtc.h"
#include "usb.h"


void gvr_task (void) {
    while (fpga_id_get() != FPGA_ID);

    cfg_init();
    flashram_init();
    usb_init();

    // fpga_reg_set(REG_SD_SCR, SD_SCR_CLOCK_MODE_25MHZ);

    while (1) {
        cfg_process();
        flashram_process();
        usb_process();
        rtc_process();
    }
}


// while (1) {
//     while (fpga_reg_get(REG_STATUS) & STATUS_BUTTON);
//     fpga_reg_set(REG_FLASH_SCR, 0x00E00000UL);
//     while (!(fpga_reg_get(REG_STATUS) & STATUS_BUTTON));
// }

// if (!(fpga_reg_get(REG_STATUS) & STATUS_BUTTON)) {
//     hw_gpio_set(GPIO_ID_LED);
//     fpga_reg_set(REG_FLASH_SCR, FLASH_SCR_ERASE_MODE);
//     for (int i = 0; i < ((2*1024)-128)*1024; i += 64*1024) {
//         fpga_mem_write(((64+14)*1024*1024) + i, 2, buffer);
//     }
//     fpga_reg_set(REG_FLASH_SCR, 0);
//     fpga_mem_read((64+14)*1024*1024, 2, buffer);
//     hw_gpio_reset(GPIO_ID_LED);
// }
// fpga_reg_set(REG_CFG_SCR, 0); //CFG_SCR_BOOTLOADER_ENABLED

// uint8_t buffer[2];

// fpga_mem_read(0*1024*1024, 16, buffer);
// fpga_mem_read(64*1024*1024, 16, buffer);
// fpga_mem_read(64*1024*1024 + 128, 16, buffer);
// buffer[0] = 0x21;
// buffer[1] = 0x37;
// buffer[2] = 0x69;
// buffer[3] = 0x42;
// fpga_mem_write(((64 + 16)*1024*1024)-(128 * 1024), 4, buffer);
// fpga_mem_read(((64 + 16)*1024*1024)-(128 * 1024)-8, 16, buffer);



