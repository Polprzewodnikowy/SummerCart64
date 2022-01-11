#include "sc64.h"


void exception_interrupt_handler (uint32_t exception_code, uint32_t interrupt_mask) {
    LOG_I("Unimplemented interrupt, mask: 0x%08lX\r\n", interrupt_mask);
    LOG_FLUSH();

    while (1);
}
