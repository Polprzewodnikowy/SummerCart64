#include "sys.h"
#include "sc64.h"


void init (void) {
    uint32_t pifram = si_io_read((io32_t *) (&PIFRAM[0x3C]));
    si_io_write((io32_t *) (&PIFRAM[0x3C]), pifram | 0x08);

    sc64_init();
}
