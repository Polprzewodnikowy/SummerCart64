#include "isv.h"
#include "usb.h"


#define IS64_TOKEN      (0x34365349)
#define IS64_OFFSET     (SDRAM_BASE + 0x03FF0000)


typedef struct {
    uint32_t ID;
    uint32_t __padding_1[4];
    uint32_t RD_PTR;
    uint32_t __padding_2[2];
    uint8_t BUFFER[(64 * 1024) - 0x20];
} is_viewer_t;


struct process {
    bool enabled;
    uint32_t current_read_pointer;
    is_viewer_t *ISV;
};

struct process p;


void isv_set_enabled (bool enabled) {
    p.enabled = enabled;
}

bool isv_get_enabled (void) {
    return p.enabled;
}


void isv_init (void) {
    p.enabled = false;
    p.current_read_pointer = 0;
    p.ISV = (is_viewer_t *) (IS64_OFFSET);
}

void process_isv (void) {
    if (!p.enabled || (p.ISV->ID != IS64_TOKEN)) {
        p.current_read_pointer = 0;
        p.ISV->RD_PTR = 0;
        return;
    }

    uint32_t read_pointer = SWAP32(p.ISV->RD_PTR);

    if ((read_pointer != p.current_read_pointer) && usb_internal_debug_tx_ready()) {
        bool wrap = read_pointer < p.current_read_pointer;
        size_t length = ((wrap ? sizeof(p.ISV->BUFFER) : read_pointer) - p.current_read_pointer);
        uint32_t address = (uint32_t) (&p.ISV->BUFFER[p.current_read_pointer]);

        if (usb_internal_debug_tx_data(INT_DBG_ID_IS_VIEWER, address, length)) {
            p.current_read_pointer = wrap ? 0 : read_pointer;
        }
    }
}
