#ifndef N64_REGS_H__
#define N64_REGS_H__

#include <inttypes.h>

#define ARRAY_ITEMS(x)          (sizeof(x) / sizeof(x[0]))

#define CPU_ADDRESS_IN_REG(x)   ((0xFFFFFFFFULL << 32) | ((uint32_t) (&(x))))

#define CPU_REG_Z0              (0)
#define CPU_REG_AT              (1)
#define CPU_REG_V0              (2)
#define CPU_REG_V1              (3)
#define CPU_REG_A0              (4)
#define CPU_REG_A1              (5)
#define CPU_REG_A2              (6)
#define CPU_REG_A3              (7)
#define CPU_REG_T0              (8)
#define CPU_REG_T1              (9)
#define CPU_REG_T2              (10)
#define CPU_REG_T3              (11)
#define CPU_REG_T4              (12)
#define CPU_REG_T5              (13)
#define CPU_REG_T6              (14)
#define CPU_REG_T7              (15)
#define CPU_REG_S0              (16)
#define CPU_REG_S1              (17)
#define CPU_REG_S2              (18)
#define CPU_REG_S3              (19)
#define CPU_REG_S4              (20)
#define CPU_REG_S5              (21)
#define CPU_REG_S6              (22)
#define CPU_REG_S7              (23)
#define CPU_REG_T8              (24)
#define CPU_REG_T9              (25)
#define CPU_REG_K0              (26)
#define CPU_REG_K1              (27)
#define CPU_REG_GP              (28)
#define CPU_REG_SP              (29)
#define CPU_REG_FP              (30)
#define CPU_REG_RA              (31)

typedef struct SP_MEM_s {
    volatile uint32_t dmem[1024];
    volatile uint32_t imem[1024];
} SP_MEM_t;

typedef struct SP_regs_s {
    volatile void *mem_addr;
    volatile void *dram_addr;
    volatile uint32_t rd_len;
    volatile uint32_t wr_len;
    volatile uint32_t status;
    volatile uint32_t dma_full;
    volatile uint32_t dma_busy;
    volatile uint32_t semaphore;
} SP_regs_t;

typedef struct DP_CMD_regs_s {
    volatile void *start;
    volatile void *end;
    volatile void *current;
    volatile uint32_t status;
    volatile uint32_t clock;
    volatile uint32_t buf_busy;
    volatile uint32_t pipe_busy;
    volatile uint32_t tmem;
} DP_CMD_regs_t;

typedef struct VI_regs_s {
    volatile uint32_t control;
    volatile void *dram_addr;
    volatile uint32_t h_width;
    volatile uint32_t v_intr;
    volatile uint32_t current_line;
    volatile uint32_t timing;
    volatile uint32_t v_sync;
    volatile uint32_t h_sync;
    volatile uint32_t h_sync_leap;
    volatile uint32_t h_limits;
    volatile uint32_t v_limits;
    volatile uint32_t color_burst;
    volatile uint32_t h_scale;
    volatile uint32_t v_scale;
} VI_regs_t;

typedef struct AI_regs_s {
    volatile void *dram_addr;
    volatile uint32_t len;
    volatile uint32_t control;
    volatile uint32_t status;
    volatile uint32_t dacrate;
    volatile uint32_t bitrate;
} AI_regs_t;

typedef struct PI_regs_s {
    volatile void *dram_addr;
    volatile uint32_t cart_addr;
    volatile uint32_t rd_len;
    volatile uint32_t wr_len;
    volatile uint32_t status;
    volatile uint32_t dom1_lat;
    volatile uint32_t dom1_pwd;
    volatile uint32_t dom1_pgs;
    volatile uint32_t dom1_rls;
    volatile uint32_t dom2_lat;
    volatile uint32_t dom2_pwd;
    volatile uint32_t dom2_pgs;
    volatile uint32_t dom2_rls;
} PI_regs_t;

#define SP_MEM_BASE                         (0xA4000000)
#define SP_REGS_BASE                        (0xA4040000)
#define DP_CMD_REGS_BASE                    (0xA4100000)
#define VI_REGS_BASE                        (0xA4400000)
#define AI_REGS_BASE                        (0xA4500000)
#define PI_REGS_BASE                        (0xA4600000)
#define DDIPL_BASE                          (0xA6000000)
#define CART_BASE                           (0xB0000000)

#define SP_MEM                              ((volatile SP_MEM_t *) SP_MEM_BASE)
#define SP                                  ((volatile SP_regs_t *) SP_REGS_BASE)
#define DP_CMD                              ((volatile DP_CMD_regs_t *) DP_CMD_REGS_BASE)
#define VI                                  ((volatile VI_regs_t *) VI_REGS_BASE)
#define AI                                  ((volatile AI_regs_t *) AI_REGS_BASE)
#define PI                                  ((volatile PI_regs_t *) PI_REGS_BASE)
#define DDIPL                               ((volatile uint32_t *) DDIPL_BASE)
#define CART                                ((volatile uint32_t *) CART_BASE)

#define SP_STATUS_HALT                      (1 << 0)
#define SP_STATUS_DMA_BUSY                  (1 << 2)
#define SP_STATUS_SET_HALT                  (1 << 1)
#define SP_STATUS_CLEAR_INTERRUPT           (1 << 3)
#define SP_DMA_BUSY                         (1 << 0)

#define DP_CMD_STATUS_XBUS_DMEM_DMA         (1 << 0)
#define DP_CMD_STATUS_PIPE_BUSY             (1 << 5)
#define DP_CMD_STATUS_CLEAR_XBUS_DMEM_DMA   (1 << 0)
#define DP_CMD_STATUS_CLEAR_FREEZE          (1 << 2)
#define DP_CMD_STATUS_CLEAR_FLUSH           (1 << 4)

#define PI_STATUS_DMA_BUSY                  (1 << 0)
#define PI_STATUS_IO_BUSY                   (1 << 1)
#define PI_STATUS_RESET_CONTROLLER          (1 << 0)
#define PI_STATUS_CLEAR_INTERRUPT           (1 << 1)

#endif
