#include <stdint.h>
#include "button.h"
#include "fpga.h"
#include "usb.h"


struct process {
    uint32_t shift;
    bool state;
    button_mode_t mode;
    bool trigger;
};


static struct process p;


bool button_get_state (void) {
    return p.state;
}

void button_set_mode (button_mode_t mode) {
    p.mode = mode;
    if (p.mode == BUTTON_MODE_NONE) {
        p.trigger = false;
    }
}

button_mode_t button_get_mode (void) {
    return p.mode;
}

void button_init (void) {
    p.shift = 0x00000000UL;
    p.state = false;
    p.mode = BUTTON_MODE_NONE;
    p.trigger = false;
}

void button_process (void) {
    usb_tx_info_t packet_info;
    uint32_t status = fpga_reg_get(REG_STATUS);
    p.shift <<= 1;
    if (status & STATUS_BUTTON) {
        p.shift |= (1 << 0);
    }
    if (!p.state && p.shift == 0xFFFFFFFFUL) {
        p.state = true;
        p.trigger = true;
    }
    if (p.state && p.shift == 0x00000000UL) {
        p.state = false;
    }
    if (p.trigger) {
        switch (p.mode) {
            case BUTTON_MODE_N64_IRQ:
                fpga_reg_set(REG_CFG_CMD, fpga_reg_get(REG_CFG_CMD) | CFG_CMD_IRQ);
                p.trigger = false;
                break;

            case BUTTON_MODE_USB_PACKET:
                usb_create_packet(&packet_info, PACKET_CMD_BUTTON_TRIGGER);
                if (usb_enqueue_packet(&packet_info)) {
                    p.trigger = false;
                }
                break;

            case BUTTON_MODE_DD_DISK_SWAP:
                // TODO: implement 64DD disk swapping
                p.trigger = false;
                break;

            default:
                p.trigger = false;
                break;
        }
    }
}
