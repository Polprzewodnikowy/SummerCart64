#ifndef IO_H__
#define IO_H__


#include <stddef.h>
#include <stdint.h>


typedef volatile uint8_t io8_t;
typedef volatile uint32_t io32_t;


#define ALIGN(value, align)         (((value) + ((typeof(value))(align) - 1)) & ~((typeof(value))(align) - 1))

#define PHYSICAL(address)           ((typeof(address)) (((io32_t) (address)) & (0x1FFFFFFFUL)))
#define UNCACHED(address)           ((typeof(address)) (((io32_t) (address)) | (0xA0000000UL)))

#define N64_RAM_SIZE                (0x00800000UL)

#define FROM_BCD(x)                 ((((x >> 4) & 0x0F) * 10) + (x & 0x0F))


typedef struct {
    io32_t DMEM[1024];
    io32_t IMEM[1024];
} sp_mem_t;

#define SP_MEM_BASE                 (0x04000000UL)
#define SP_MEM                      ((sp_mem_t *) SP_MEM_BASE)


typedef struct {
    io32_t PADDR;
    io32_t MADDR;
    io32_t RD_LEN;
    io32_t WR_LEN;
    io32_t SR;
    io32_t DMA_FULL;
    io32_t DMA_BUSY;
    io32_t SEMAPHORE;
    io32_t __reserved[0xFFF8];
    io32_t PC;
} sp_regs_t;

#define SP_BASE                     (0x04040000UL)
#define SP                          ((sp_regs_t *) SP_BASE)

#define SP_SR_HALT                  (1 << 0)
#define SP_SR_BROKE                 (1 << 1)
#define SP_SR_DMA_BUSY              (1 << 2)
#define SP_SR_DMA_FULL              (1 << 3)
#define SP_SR_IO_FULL               (1 << 4)
#define SP_SR_SSTEP                 (1 << 5)
#define SP_SR_INTR_BREAK            (1 << 6)
#define SP_SR_SIG0                  (1 << 7)
#define SP_SR_SIG1                  (1 << 8)
#define SP_SR_SIG2                  (1 << 9)
#define SP_SR_SIG3                  (1 << 10)
#define SP_SR_SIG4                  (1 << 11)
#define SP_SR_SIG5                  (1 << 12)
#define SP_SR_SIG6                  (1 << 13)
#define SP_SR_SIG7                  (1 << 14)
#define SP_SR_CLR_HALT              (1 << 0)
#define SP_SR_SET_HALT              (1 << 1)
#define SP_SR_CLR_BROKE             (1 << 2)
#define SP_SR_CLR_INTR              (1 << 3)
#define SP_SR_SET_INTR              (1 << 4)
#define SP_SR_CLR_SSTEP             (1 << 5)
#define SP_SR_SET_SSTEP             (1 << 6)
#define SP_SR_CLR_INTR_BREAK        (1 << 7)
#define SP_SR_SET_INTR_BREAK        (1 << 8)
#define SP_SR_CLR_SIG0              (1 << 9)
#define SP_SR_SET_SIG0              (1 << 10)
#define SP_SR_CLR_SIG1              (1 << 11)
#define SP_SR_SET_SIG1              (1 << 12)
#define SP_SR_CLR_SIG2              (1 << 13)
#define SP_SR_SET_SIG2              (1 << 14)
#define SP_SR_CLR_SIG3              (1 << 15)
#define SP_SR_SET_SIG3              (1 << 16)
#define SP_SR_CLR_SIG4              (1 << 17)
#define SP_SR_SET_SIG4              (1 << 18)
#define SP_SR_CLR_SIG5              (1 << 19)
#define SP_SR_SET_SIG5              (1 << 20)
#define SP_SR_CLR_SIG6              (1 << 21)
#define SP_SR_SET_SIG6              (1 << 22)
#define SP_SR_CLR_SIG7              (1 << 23)
#define SP_SR_SET_SIG7              (1 << 24)


typedef struct {
    io32_t START;
    io32_t END;
    io32_t CURRENT;
    io32_t SR;
    io32_t CLOCK;
    io32_t BUF_BUSY;
    io32_t PIPE_BUSY;
    io32_t TMEM;
} dpc_regs_t;

#define DPC_BASE                    (0x04100000UL)
#define DPC                         ((dpc_regs_t *) DPC_BASE)

#define DPC_SR_XBUS_DMEM_DMA        (1 << 0)
#define DPC_SR_FREEZE               (1 << 1)
#define DPC_SR_FLUSH                (1 << 2)
#define DPC_SR_START_GCLK           (1 << 3)
#define DPC_SR_TMEM_BUSY            (1 << 4)
#define DPC_SR_PIPE_BUSY            (1 << 5)
#define DPC_SR_CMD_BUSY             (1 << 6)
#define DPC_SR_CBUF_READY           (1 << 7)
#define DPC_SR_DMA_BUSY             (1 << 8)
#define DPC_SR_END_VALID            (1 << 9)
#define DPC_SR_START_VALID          (1 << 10)
#define DPC_SR_CLR_XBUS_DMEM_DMA    (1 << 0)
#define DPC_SR_SET_XBUS_DMEM_DMA    (1 << 1)
#define DPC_SR_CLR_FREEZE           (1 << 2)
#define DPC_SR_SET_FREEZE           (1 << 3)
#define DPC_SR_CLR_FLUSH            (1 << 4)
#define DPC_SR_SET_FLUSH            (1 << 5)
#define DPC_SR_CLR_TMEM_CTR         (1 << 6)
#define DPC_SR_CLR_PIPE_CTR         (1 << 7)
#define DPC_SR_CLR_CMD_CTR          (1 << 8)
#define DPC_SR_CLR_CLOCK_CTR        (1 << 9)


typedef struct {
    io32_t CR;
    io32_t MADDR;
    io32_t H_WIDTH;
    io32_t V_INTR;
    io32_t CURR_LINE;
    io32_t TIMING;
    io32_t V_SYNC;
    io32_t H_SYNC;
    io32_t H_SYNC_LEAP;
    io32_t H_LIMITS;
    io32_t V_LIMITS;
    io32_t COLOR_BURST;
    io32_t H_SCALE;
    io32_t V_SCALE;
} vi_regs_t;

#define VI_BASE                     (0x04400000UL)
#define VI                          ((vi_regs_t *) VI_BASE)

#define VI_CR_TYPE_16               (2 << 0)
#define VI_CR_TYPE_32               (3 << 0)
#define VI_CR_GAMMA_DITHER_ON       (1 << 2)
#define VI_CR_GAMMA_ON              (1 << 3)
#define VI_CR_DIVOT_ON              (1 << 4)
#define VI_CR_SERRATE_ON            (1 << 6)
#define VI_CR_ANTIALIAS_0           (1 << 8)
#define VI_CR_ANTIALIAS_1           (1 << 9)
#define VI_CR_PIXEL_ADVANCE_0       (1 << 12)
#define VI_CR_PIXEL_ADVANCE_1       (1 << 13)
#define VI_CR_PIXEL_ADVANCE_2       (1 << 14)
#define VI_CR_PIXEL_ADVANCE_3       (1 << 15)
#define VI_CR_DITHER_FILTER_ON      (1 << 16)

#define VI_CURR_LINE_FIELD          (1 << 0)


typedef struct {
    io32_t MADDR;
    io32_t LEN;
    io32_t CR;
    io32_t SR;
    io32_t DACRATE;
    io32_t BITRATE;
} ai_regs_t;

#define AI_BASE                     (0x04500000UL)
#define AI                          ((ai_regs_t *) AI_BASE)

#define AI_SR_DMA_BUSY              (1 << 30)
#define AI_SR_FIFO_FULL             (1 << 31)
#define AI_CR_DMA_ON                (1 << 0)


typedef struct {
    io32_t MADDR;
    io32_t PADDR;
    io32_t RDMA;
    io32_t WDMA;
    io32_t SR;
    struct {
        io32_t LAT;
        io32_t PWD;
        io32_t PGS;
        io32_t RLS;
    } DOM[2];
} pi_regs_t;

#define PI_BASE                     (0x04600000UL)
#define PI                          ((pi_regs_t *) PI_BASE)

#define PI_SR_DMA_BUSY              (1 << 0)
#define PI_SR_IO_BUSY               (1 << 1)
#define PI_SR_DMA_ERROR             (1 << 2)
#define PI_SR_RESET                 (1 << 0)
#define PI_SR_CLR_INTR              (1 << 1)


typedef struct {
    io32_t MADDR;
    io32_t RDMA;
    io32_t __reserved_1;
    io32_t __reserved_2;
    io32_t WDMA;
    io32_t __reserved_3;
    io32_t SR;
} si_regs_t;

#define SI_BASE                     (0x04800000UL)
#define SI                          ((si_regs_t *) SI_BASE)

#define SI_SR_DMA_BUSY              (1 << 0)
#define SI_SR_IO_BUSY               (1 << 1)
#define SI_SR_DMA_ERROR             (1 << 3)
#define SI_SR_INTERRUPT             (1 << 12)
#define SI_SR_CLEAR_INTERRUPT       (0)


#define ROM_DDIPL_BASE              (0x06000000UL)
#define ROM_DDIPL                   ((io32_t *) ROM_DDIPL_BASE)


#define ROM_CART_BASE               (0x10000000UL)
#define ROM_CART                    ((io32_t *) ROM_CART_BASE)


#define PIFRAM_BASE                 (0x1FC007C0UL)
#define PIFRAM                      ((io8_t *) PIFRAM_BASE)

#define PIFRAM_STATUS               (&PIFRAM[0x3C])

#define PIFRAM_TERMINATE_BOOT       (1 << 3)


typedef struct {
    uint32_t tv_type;
    uint32_t device_type;
    uint32_t device_base;
    uint32_t reset_type;
    uint32_t cic_id;
    uint32_t version;
    uint32_t mem_size;
    uint8_t app_nmi_buffer[64];
    uint32_t __reserved_1[37];
    uint32_t mem_size_6105;
} os_info_t;

#define OS_INFO_BASE                (0x80000300UL)
#define OS_INFO                     ((os_info_t *) OS_INFO_BASE)

#define OS_INFO_RESET_TYPE_COLD     (0)
#define OS_INFO_RESET_TYPE_NMI      (1)


uint32_t c0_count (void);
void delay_ms (int ms);
uint32_t cpu_io_read (io32_t *address);
void cpu_io_write (io32_t *address, uint32_t value);
void pi_io_config (uint8_t page_size, uint8_t latency, uint8_t pulse_width, uint8_t release);
uint32_t pi_busy (void);
uint32_t pi_io_read (io32_t *address);
void pi_io_write (io32_t *address, uint32_t value);
void pi_dma_read (io32_t *address, void *buffer, size_t length);
void pi_dma_write (io32_t *address, void *buffer, size_t length);
uint32_t si_busy (void);
uint32_t si_io_read (io32_t *address);
void si_io_write (io32_t *address, uint32_t value);
void cache_data_hit_writeback_invalidate (void *address, size_t length);
void cache_data_hit_writeback (void *address, size_t length);
void cache_inst_hit_invalidate (void *address, size_t length);


#endif
