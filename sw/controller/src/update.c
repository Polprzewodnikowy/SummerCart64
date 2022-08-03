#include <stdint.h>
#include "hw.h"
#include "update.h"
#include "usb.h"


static uint32_t update_mcu_address;
static uint32_t update_mcu_length;
static uint32_t update_fpga_address;
static uint32_t update_fpga_length;


uint32_t update_backup (uint32_t address) {
    // TODO: create backup
    return 0;
}

update_error_t update_prepare (uint32_t address, uint32_t length) {
    // TODO: validate image
    update_mcu_address = 0;
    update_mcu_length = 0;
    update_fpga_address = 0;
    update_fpga_length = 0;
    return UPDATE_OK;
}

void update_start (void) {
    uint32_t parameters[5] = {
        HW_UPDATE_START_MAGIC,
        update_mcu_address,
        update_mcu_length,
        update_fpga_address,
        update_fpga_length,
    };
    hw_loader_reset(parameters);
}

void update_notify_done (void) {
    uint32_t parameters[5];
    usb_tx_info_t packet;

    hw_loader_get_parameters(parameters);

    if (parameters[0] == HW_UPDATE_DONE_MAGIC) {
        hw_loader_clear_parameters();
        usb_create_packet(&packet, PACKET_CMD_UPDATE_DONE);
        usb_enqueue_packet(&packet);
    }
}
