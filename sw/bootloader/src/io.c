#include "io.h"


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

void pi_dma_read (io32_t *address, void *buffer, size_t length) {
    io_write(&PI->PADDR, (uint32_t) (PHYSICAL(address)));
    io_write(&PI->MADDR, (uint32_t) (PHYSICAL(buffer)));
    io_write(&PI->WDMA, length - 1);
    while (pi_busy());
}

void pi_dma_write (io32_t *address, void *buffer, size_t length) {
    io_write(&PI->PADDR, (uint32_t) (PHYSICAL(address)));
    io_write(&PI->MADDR, (uint32_t) (PHYSICAL(buffer)));
    io_write(&PI->RDMA, length - 1);
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

static void cache_operation (uint8_t operation, uint8_t line_size, void *address, size_t length) {
    uint32_t cache_address = (((uint32_t) (address)) & (~(line_size - 1)));
    while (cache_address < ((uint32_t) (address) + length)) {
        asm volatile (
            "cache %[operation], (%[cache_address]) \n" ::
            [operation] "i" (operation),
            [cache_address] "r" (cache_address)
        );
        cache_address += line_size;
    }    
}

void cache_data_hit_invalidate (void *address, size_t length) {
    cache_operation (0x11, 16, address, length);
}

void cache_data_hit_writeback (void *address, size_t length) {
    cache_operation (0x19, 16, address, length);
}
