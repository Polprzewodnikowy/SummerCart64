#include "isv.h"
#include "usb.h"


#define ISV_REGS_SIZE       (0x20)
#define ISV_BUFFER_SIZE     ((64 * 1024) - ISV_REGS_SIZE)
#define ISV_DEFAULT_OFFSET  (0x03FF0000UL)


struct process {
    bool ready;
};

static struct process p;


static void isv_set_ready (void) {
    p.ready = true;
}

void isv_set_enabled (bool enabled) {
    if (enabled) {
        CFG->SCR |= CFG_SCR_ISV_EN;
        CFG->ISV_CURRENT_RD_PTR = CFG->ISV_RD_PTR;
        p.ready = true;
    } else {
        CFG->SCR &= ~(CFG_SCR_ISV_EN);
    }
}

bool isv_get_enabled (void) {
    return (CFG->SCR & CFG_SCR_ISV_EN);
}


void isv_init (void) {
    CFG->ISV_OFFSET = ISV_DEFAULT_OFFSET;
    p.ready = true;
}


void process_isv (void) {
    if (p.ready && (CFG->SCR & CFG_SCR_ISV_EN)) {
        uint16_t read_pointer = CFG->ISV_RD_PTR;
        uint16_t current_read_pointer = CFG->ISV_CURRENT_RD_PTR;

        if (read_pointer != current_read_pointer) {
            bool wrap = read_pointer < current_read_pointer;

            uint32_t length = ((wrap ? ISV_BUFFER_SIZE : read_pointer) - current_read_pointer);
            uint32_t offset = CFG->ISV_OFFSET + ISV_REGS_SIZE + current_read_pointer;

            usb_event_t event;
            event.id = EVENT_ID_IS_VIEWER;
            event.trigger = CALLBACK_SDRAM_READ;
            event.callback = isv_set_ready;
            uint32_t data[2] = { length, offset };

            if (usb_put_event(&event, data, sizeof(data))) {
                CFG->ISV_CURRENT_RD_PTR = wrap ? 0 : read_pointer;
                p.ready = false;
            }
        }
    }
}
