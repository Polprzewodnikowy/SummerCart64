#include "isv.h"
#include "usb.h"


typedef struct {
    io32_t __padding_1[5];
    io32_t RD_PTR;
    io32_t __padding_2[2];
    io8_t BUFFER[(64 * 1024) - 0x20];
} isv_t;

#define ISV_BASE            (SDRAM_BASE + 0x03FF0000UL)
#define ISV                 ((isv_t *) ISV_BASE)


struct process {
    bool enabled;
    bool ready;
    uint16_t current_read_pointer;
};

static struct process p;


static void isv_set_ready (void) {
    p.ready = true;
}

void isv_set_enabled (bool enabled) {
    if (enabled) {
        CFG->SCR |= CFG_SCR_WRITES_ON_RESET_EN;
        ISV->RD_PTR = SWAP32(0);
        p.enabled = true;
        p.ready = true;
        p.current_read_pointer = 0;
    } else {
        CFG->SCR &= ~(CFG_SCR_WRITES_ON_RESET_EN);
        p.enabled = false;
    }
}

bool isv_get_enabled (void) {
    return p.enabled;
}


void isv_init (void) {
    p.enabled = false;
}


void process_isv (void) {
    if (p.enabled && p.ready) {
        uint16_t read_pointer = (uint16_t) (SWAP32(ISV->RD_PTR));

        if (read_pointer != p.current_read_pointer) {
            bool wrap = read_pointer < p.current_read_pointer;

            uint32_t length = ((wrap ? sizeof(ISV->BUFFER) : read_pointer) - p.current_read_pointer);
            uint32_t offset = (((uint32_t) (&ISV->BUFFER[p.current_read_pointer])) & 0x0FFFFFFF);

            usb_event_t event;
            event.id = EVENT_ID_IS_VIEWER;
            event.trigger = CALLBACK_SDRAM_READ;
            event.callback = isv_set_ready;
            uint32_t data[2] = { length, offset };

            if (usb_put_event(&event, data, sizeof(data))) {
                p.current_read_pointer = wrap ? 0 : read_pointer;
                p.ready = false;
            }
        }
    }
}
