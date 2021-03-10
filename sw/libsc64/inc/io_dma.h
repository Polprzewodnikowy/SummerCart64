#ifndef SC64_IO_DMA_H__
#define SC64_IO_DMA_H__


#include <stdint.h>
#include <stdlib.h>


void sc64_io_dma_init(void);

void sc64_io_write(volatile uint32_t *pi_address, uint32_t value);
uint32_t sc64_io_read(volatile uint32_t *pi_address);

void sc64_dma_write(void *ram_address, volatile uint32_t *pi_address, size_t length);
void sc64_dma_read(void *ram_address, volatile uint32_t *pi_address, size_t length);


#endif
