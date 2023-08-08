#include "io.h"
#include "vr4300.h"


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

void cache_data_hit_writeback_invalidate (void *address, size_t length) {
    cache_operation(HIT_WRITE_BACK_INVALIDATE_D, CACHE_LINE_SIZE_D, address, length);
}

void cache_data_hit_writeback (void *address, size_t length) {
    cache_operation(HIT_WRITE_BACK_D, CACHE_LINE_SIZE_D, address, length);
}

void cache_inst_hit_invalidate (void *address, size_t length) {
    cache_operation(HIT_INVALIDATE_I, CACHE_LINE_SIZE_I, address, length);
}

uint32_t c0_count (void) {
    uint32_t value;
    asm volatile (
        "mfc0 %[value], $9 \n" :
        [value] "=r" (value)
    );
    return value;
}

void delay_ms (int ms) {
    uint64_t start = c0_count();
    uint64_t end = start + (ms * ((93750000 / 2) / 1000));
    uint64_t current;
    do {
        current = c0_count();
        if (current < start) {
            current += 0x100000000ULL;
        }
    } while (current < end);
}

uint32_t cpu_io_read (io32_t *address) {
    io32_t *uncached = UNCACHED(address);
    uint32_t value = *uncached;
    return value;
}

void cpu_io_write (io32_t *address, uint32_t value) {
    io32_t *uncached = UNCACHED(address);
    *uncached = value;
}

void pi_io_config (uint8_t page_size, uint8_t latency, uint8_t pulse_width, uint8_t release) {
    for (int domain = 0; domain < 2; domain += 1) {
        cpu_io_write(&PI->DOM[domain].PGS, page_size);
        cpu_io_write(&PI->DOM[domain].LAT, latency);
        cpu_io_write(&PI->DOM[domain].PWD, pulse_width);
        cpu_io_write(&PI->DOM[domain].RLS, release);
    }
}

uint32_t pi_busy (void) {
    return (cpu_io_read(&PI->SR) & (PI_SR_IO_BUSY | PI_SR_DMA_BUSY));
}

uint32_t pi_io_read (io32_t *address) {
    return cpu_io_read(address);
}

void pi_io_write (io32_t *address, uint32_t value) {
    cpu_io_write(address, value);
    while (pi_busy());
}

void pi_dma_read (io32_t *address, void *buffer, size_t length) {
    cache_data_hit_writeback_invalidate(buffer, length);
    cpu_io_write(&PI->PADDR, (uint32_t) (PHYSICAL(address)));
    cpu_io_write(&PI->MADDR, (uint32_t) (PHYSICAL(buffer)));
    cpu_io_write(&PI->WDMA, length - 1);
    while (pi_busy());
}

void pi_dma_write (io32_t *address, void *buffer, size_t length) {
    cache_data_hit_writeback(buffer, length);
    cpu_io_write(&PI->PADDR, (uint32_t) (PHYSICAL(address)));
    cpu_io_write(&PI->MADDR, (uint32_t) (PHYSICAL(buffer)));
    cpu_io_write(&PI->RDMA, length - 1);
    while (pi_busy());
}

uint32_t si_busy (void) {
    return (cpu_io_read(&SI->SR) & (SI_SR_IO_BUSY | SI_SR_DMA_BUSY));
}

uint32_t si_io_read (io32_t *address) {
    return cpu_io_read(address);
}

void si_io_write (io32_t *address, uint32_t value) {
    cpu_io_write(address, value);
    while (si_busy());
}
