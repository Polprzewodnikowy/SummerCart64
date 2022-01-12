#include "exception.h"
#include "io.h"
#include "sc64.h"


void init (void) {
    uint32_t pifram = si_io_read((io32_t *) (&PIFRAM[0x3C]));
    si_io_write((io32_t *) (&PIFRAM[0x3C]), pifram | 0x08);

    exception_install();
    exception_enable_watchdog();
    exception_enable_interrupts();

    sc64_init();
}

void deinit (void) {
    exception_disable_interrupts();
    exception_disable_watchdog();
}
