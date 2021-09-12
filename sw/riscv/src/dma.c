#include "dma.h"


bool dma_busy (void) {
    return DMA->SCR & DMA_SCR_BUSY;
}

void dma_start (uint32_t memory_address, size_t length, enum dma_id id, enum dma_dir dir) {
    DMA->MADDR = memory_address;
    DMA->ID_LEN = ((id & 0x03) << 30) | (length & 0x3FFFFFFF);
    DMA->SCR = ((dir == DMA_DIR_TO_SDRAM) ? DMA_SCR_DIR : 0) | DMA_SCR_START;
}

void dma_stop (void) {
    DMA->SCR = DMA_SCR_STOP;
}


void dma_init (void) {
    dma_stop();
}


void process_dma (void) {

}
