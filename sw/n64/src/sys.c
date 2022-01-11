#include "sys.h"


uint32_t io_read (io32_t *address) {
    io32_t *uncached = UNCACHED(address);
    uint32_t value = *uncached;
    return value;
}

void io_write (io32_t *address, uint32_t value) {
    io32_t *uncached = UNCACHED(address);
    *uncached = value;
}

uint32_t pi_busy (void) {
    return (io_read(&PI->SR) & (PI_SR_IO_BUSY | PI_SR_DMA_BUSY));
}

uint32_t pi_io_read (io32_t *address) {
    return io_read(address);
}

void pi_io_write (io32_t *address, uint32_t value) {
    io_write(address, value);
    while (pi_busy());
}

uint32_t si_busy (void) {
    return (io_read(&SI->SR) & (SI_SR_IO_BUSY | SI_SR_DMA_BUSY));
}

uint32_t si_io_read (io32_t *address) {
    return io_read(address);
}

void si_io_write (io32_t *address, uint32_t value) {
    io_write(address, value);
    while (si_busy());
}
