#include "sys.h"


void c0_set_status (uint32_t status) {
    asm volatile (
        ".set noat \n"
        ".set noreorder \n"
        "mtc0 %[status], $12 \n"
        "nop \n"
        :
        : [status] "r" (status)
    );
}

uint32_t c0_get_count (void) {
    uint32_t count;
    asm volatile (
        ".set noat \n"
        ".set noreorder \n"
        "mfc0 %[count], $9 \n"
        "nop \n"
        : [count] "=r" (count)
    );
    return count;
}

void wait_ms (uint32_t ms) {
    uint32_t start = c0_get_count();
    while (c0_get_count() - start < (ms * ((CPU_FREQUENCY / 2) / 1000)));
}

uint32_t io_read (io32_t *address) {
    io32_t *uncached = UNCACHED(address);
    asm volatile ("" : : : "memory");
    uint32_t value = *uncached;
    asm volatile ("" : : : "memory");
    return value;
}

void io_write (io32_t *address, uint32_t value) {
    io32_t *uncached = UNCACHED(address);
    asm volatile ("" : : : "memory");
    *uncached = value;
    asm volatile ("" : : : "memory");
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
