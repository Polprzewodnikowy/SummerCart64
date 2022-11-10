#include <stdint.h>
#include "button.h"
#include "dd.h"
#include "fpga.h"
#include "usb.h"


#define BUTTON_COUNTER_TRIGGER_ON   (64)
#define BUTTON_COUNTER_TRIGGER_OFF  (0)


struct process {
    uint8_t counter;
    bool state;
    button_mode_t mode;
    bool trigger;
};


static struct process p;


bool button_get_state (void) {
    return p.state;
}

bool button_set_mode (button_mode_t mode) {
    if (mode > BUTTON_MODE_DD_DISK_SWAP) {
        return true;
    }
    p.mode = mode;
    if (p.mode == BUTTON_MODE_NONE) {
        p.trigger = false;
    }
    return false;
}

button_mode_t button_get_mode (void) {
    return p.mode;
}

void button_init (void) {
    p.counter = 0;
    p.state = false;
    p.mode = BUTTON_MODE_NONE;
    p.trigger = false;
}

void button_process (void) {
    usb_tx_info_t packet_info;
    uint32_t status = fpga_reg_get(REG_CFG_SCR);
    if (status & CFG_SCR_BUTTON_STATE) {
        if (p.counter < BUTTON_COUNTER_TRIGGER_ON) {
            p.counter += 1;
        }
    } else {
        if (p.counter > BUTTON_COUNTER_TRIGGER_OFF) {
            p.counter -= 1;
        }
    }
    if (!p.state && p.counter == BUTTON_COUNTER_TRIGGER_ON) {
        p.state = true;
        p.trigger = true;
    }
    if (p.state && p.counter == BUTTON_COUNTER_TRIGGER_OFF) {
        p.state = false;
    }
    if (p.trigger) {
        switch (p.mode) {
            case BUTTON_MODE_N64_IRQ:
                fpga_reg_set(REG_CFG_CMD, CFG_CMD_IRQ);
                p.trigger = false;
                break;

            case BUTTON_MODE_USB_PACKET:
                usb_create_packet(&packet_info, PACKET_CMD_BUTTON_TRIGGER);
                if (usb_enqueue_packet(&packet_info)) {
                    p.trigger = false;
                }
                break;

            case BUTTON_MODE_DD_DISK_SWAP:
                dd_handle_button();
                p.trigger = false;
                break;

            default:
                p.trigger = false;
                break;
        }
    }
}
