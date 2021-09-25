#ifndef DMA_H__
#define DMA_H__


#include "sys.h"


enum dma_dir {
    DMA_DIR_TO_SDRAM,
    DMA_DIR_FROM_SDRAM,
};

enum dma_id {
    DMA_ID_USB = 0,
    DMA_ID_SD = 1,
};


bool dma_busy (void);
void dma_start (uint32_t memory_address, size_t length, enum dma_id id, enum dma_dir dir);
void dma_stop (void);
void dma_init (void);
void process_dma (void);


#endif
