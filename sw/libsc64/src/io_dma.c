#include "io_dma.h"


#ifndef LIBDRAGON
#ifndef LIBULTRA
#error "Please specify used library with -DLIBDRAGON or -DLIBULTRA"
#endif
#endif


#ifdef LIBDRAGON
#include <libdragon.h>
#endif


#ifdef LIBULTRA
#include <ultra64.h>

static OSMesg pi_message;
static OSIoMesg pi_io_message;
static OSMesgQueue pi_message_queue;
#endif


void sc64_io_dma_init(void) {
#ifdef LIBULTRA
    osCreateMesgQueue(&pi_message_queue, &pi_message, 1);
#endif
}

void sc64_io_write(volatile uint32_t *pi_address, uint32_t value) {
#ifdef LIBDRAGON
    io_write((uint32_t) pi_address, value);
#elif LIBULTRA
    osPiWriteIo((u32) pi_address, value);
#endif
}

uint32_t sc64_io_read(volatile uint32_t *pi_address) {
#ifdef LIBDRAGON
    return io_read((uint32_t) pi_address);
#elif LIBULTRA
    u32 value;
    osPiReadIo((u32) pi_address, &value);
    return value;
#endif
}

void sc64_dma_write(void *ram_address, volatile uint32_t *pi_address, size_t length) {
#ifdef LIBDRAGON
    data_cache_hit_writeback_invalidate(ram_address, length);
    dma_write(ram_address, (uint32_t) pi_address, length);
#elif LIBULTRA
    osWritebackDCache(ram_address, length);
    osPiStartDma(&pi_io_message, OS_MESG_PRI_NORMAL, OS_WRITE, (u32) pi_address, ram_address, length, &pi_message_queue);
    osRecvMesg(&pi_message_queue, NULL, OS_MESG_BLOCK);
#endif
}

void sc64_dma_read(void *ram_address, volatile uint32_t *pi_address, size_t length) {
#ifdef LIBDRAGON
    dma_read(ram_address, (uint32_t) pi_address, length);
    data_cache_hit_writeback_invalidate(ram_address, length);
#elif LIBULTRA
    osPiStartDma(&pi_io_message, OS_MESG_PRI_NORMAL, OS_READ, (u32) pi_address, ram_address, length, &pi_message_queue);
    osRecvMesg(&pi_message_queue, NULL, OS_MESG_BLOCK);
    osInvalDCache(ram_address, length);
#endif
}
