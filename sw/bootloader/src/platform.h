#ifndef PLATFORM_H__
#define PLATFORM_H__


#include <stddef.h>
#include <stdint.h>

#include <libdragon.h>


#define TRUE    (1)
#define FALSE   (0)

#define __IO    volatile

typedef uint32_t reg_t;

#define platform_cache_writeback(rdram, length)     data_cache_hit_writeback_invalidate(rdram, length)
#define platform_cache_invalidate(rdram, length)    data_cache_hit_invalidate(rdram, length)
#define platform_pi_io_write(pi, value)             io_write((uint32_t) (pi), value)
#define platform_pi_io_read(pi)                     io_read((uint32_t) (pi))
#define platform_pi_dma_write(rdram, pi, length)    dma_write(rdram, (uint32_t) (pi), length)
#define platform_pi_dma_read(rdram, pi, length)     dma_read(rdram, (uint32_t) (pi), length)


#endif
